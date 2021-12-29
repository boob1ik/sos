#include <arch.h>
#include "epit.h"
#include <mem\vm.h>

// Для iMX6Q используем таймер EPIT1 в качестве таймера ядра:
// плюсы: работает в режиме низкого потребления
// Для диспетчера задач и подсчета времени подходит только режим его тактирования
// внешним кварцем 32768 Hz, поэтому:
// - поддержка плавной настройки на нс не работает,
// - таймер настраивается на работу с тиком 1/32768 и свободным счетом
// - период прерывания подсчета времени и диспетчеризации делается
//   близким к затребованному в вызове инициализации, однако обеспечивается
//   отсутствие набегания ошибки подсчета времени за счет применения специальных методов
// - моменты прерываний задаются на каждом прерывании новым значением сравнения
//   (в методе timer_event, который должен вызываться по прерыванию)
// - также это позволяет задавать прерывания по событиям, которые должны наступить раньше
//   истечения периода (методом timer_set_event)

//  ВНИМАНИЕ! EPIT - 32-х разрядный таймер обратного счета,
//  поэтому от Vybrid код модуля таймера немного отличается

static atomic_state_t syn;

static kernel_time_t rtime[2];
static int rtime_id;
static uint64_t rtime_time_ns;

static kernel_time_t rtime_to_set;
static int rtime_flip_id;


static uint64_t time_ns;
static uint64_t event_planned_ns;
static uint64_t event_period_ns = 0;

static uint32_t last_counter;
static uint32_t last_remainder; // Остаток в тиках N = 0-63 тика с периодом 1/32768 = N*0,001953125 с
static uint32_t compare;
static uint32_t event_period_ticks;

static const uint32_t ns_per_tick = 30517; // 1/32768 = 0,000030517578125
static const uint32_t time_ns_64 = 1953125;
//static const uint32_t period_ns_remainder = 578125; // 1/32768 = 0,000030517578125
static const uint32_t small_lyambda_ticks = 2;


int timer_init(uint64_t period_ns) {
    uint64_t tmp = period_ns, i = 0, j;
    EPIT1->CR = 0; // disable
    EPIT1->CR = 0x10000; // soft reset
    while(EPIT1->CR & 0x10000);

    EPIT1->CR = 0x3280002;
    EPIT1->SR = 1;

    // выполняем быстрое деление на период ns_per_tick для вычисления числа тиков таймера
    // в два сдвига с умножениеми и добавкой до минимум ns (чуть больше можно)
    tmp += 32768;
    while(1) {
        j = tmp >> 15;
        if(j == 0) break;
        tmp = tmp - j * ns_per_tick;
        i += j;
    }
    tmp = i * ns_per_tick;
    while(tmp < period_ns) {
        tmp += ns_per_tick;
        i++;
    }

    event_period_ticks = i;
    time_ns = 0;
    rtime_time_ns = 0;
    rtime_id = 0;
    rtime_flip_id = 0;
    rtime[rtime_id].tv_sec = 0;
    rtime[rtime_id].tv_nsec = 0;
    event_period_ns = tmp;
    event_planned_ns = tmp;
    atomic_state_init(&syn);
    last_counter = 0xffffffff;
    compare = last_counter - event_period_ticks;
    last_remainder = 0;
    EPIT1->CMPR = compare;
    return OK;
}

uint64_t timer_get_period() {
    return event_period_ns;
}

int timer_enable() {
    if(event_period_ns == 0) {
        return ERR;
    }
    EPIT1->CR |= 0x5;  // enable
    return OK;
}

int timer_disable() {
    EPIT1->CR &= ~0x5; // disable
    return OK;
}

int timer_set_event(uint64_t event_time_ns) {
    uint64_t tns = event_planned_ns;
    if(event_time_ns + (ns_per_tick << 2) >= tns) {
        // текущее событие будет раньше
        // учитывая коррекцию (ns_per_tick << 1) по времени работы данного кода
        // в защищенном контексте
        return ERR;
    }
    tns = tns - event_time_ns; // корректировка по времени на уменьшение
    uint64_t i = 0, j, tmp = tns;
    // аналогично инициализации - "быстрая" оценка
    // выполняем быстрое деление на период ns_per_tick для вычисления числа тиков таймера
    // (чуть больше можно)
    while(1) {
        j = tmp >> 15;
        if(j == 0) break;
        tmp = tmp - j * ns_per_tick;
        i += j;
    }
    tmp = i * ns_per_tick;
    while(tmp < tns) {
        tmp += ns_per_tick;
        i++;
    }
    uint32_t cnt = EPIT1->CNR;
    cnt = cnt - EPIT1->CMPR; // текущее число тиков до прерывания
    if(i >= cnt - small_lyambda_ticks) {
        // текущее событие будет раньше
        return ERR;
    }
    event_planned_ns = event_time_ns;
    EPIT1->CMPR = EPIT1->CMPR + i; // устанавливаем на более раннее срабатывание
    return OK;
}

void timer_event() {
    time_update();
    // события отвязаны от подсчета времени, подсчет времени всегда точный,
    // а события задаются с приемлемой погрешностью и предназначены
    // как для периодических прерываний так и для сокращения времени до прерывания
    // по timer_set_event
    event_planned_ns = time_ns + event_period_ns;
    EPIT1->CMPR = EPIT1->CNR - event_period_ticks;
    EPIT1->SR = 1; // clear compare event
    // timer_set_event(time_ns + (ns_per_tick << 3)); // TODO uncomment to test
}

void time_update() {
    writer_begin(&syn);
    uint64_t t = time_ns;
    uint32_t cnt = EPIT1->CNR;
    uint32_t tmp = last_counter - cnt + last_remainder;
    last_counter = cnt;
    last_remainder = tmp & 0x3f;
    tmp >>= 6;
    t += tmp * time_ns_64;
    time_ns = t;

    if(rtime_id != rtime_flip_id) {
        rtime_id = rtime_flip_id;
        rtime[rtime_id].tv_nsec = rtime_to_set.tv_nsec;
        rtime[rtime_id].tv_sec = rtime_to_set.tv_sec;
    }
    rtime[rtime_id].tv_nsec += time_ns - rtime_time_ns;
    if(rtime[rtime_id].tv_nsec >= 1000000000) {
        rtime[rtime_id].tv_nsec -= 1000000000;
        rtime[rtime_id].tv_sec++;
    }
    rtime_time_ns = time_ns;
    writer_commit(&syn);
}

uint64_t systime() {
    uint64_t t = 0;
    atomic_state_t temp;
    do {
        if(reader_begin(&syn, &temp) == ERR) continue;
        t = time_ns;
        uint32_t tmp = last_counter - EPIT1->CNR + last_remainder;
        t += ns_per_tick * tmp;
    } while (reader_commit(&syn, &temp) == ERR);
    return t;
}

int time_get(int clock_id, kernel_time_t *tv) {
    if(clock_id == OS_CLOCK_MONOTONIC) {
        tv->tv_sec = 0;
        tv->tv_nsec = systime();
        return OK;
    }
    if(clock_id != OS_CLOCK_REALTIME) {
        return ERR;
    }
    kernel_time_t t = {0,0};
    atomic_state_t temp;
    do {
        if(reader_begin(&syn, &temp) == ERR) continue;
        t.tv_sec = rtime[rtime_id].tv_sec;
        t.tv_nsec = rtime[rtime_id].tv_nsec;
        uint32_t tmp = last_counter - EPIT1->CNR + last_remainder;
        t.tv_nsec += ns_per_tick * tmp;
        while(t.tv_nsec >= 1000000000) {
            t.tv_nsec -= 1000000000;
            t.tv_sec++;
        }
    } while (reader_commit(&syn, &temp) == ERR);
    tv->tv_nsec = t.tv_nsec;
    tv->tv_sec = t.tv_sec;
    return OK;
}

int time_set(int clock_id, kernel_time_t *tv) {
    if(clock_id != OS_CLOCK_REALTIME) {
        return ERR;
    }
    if(rtime_flip_id != rtime_id) {
        // предыдущий вызов time_set еще не завершен установкой времени в time_update
        return ERR_BUSY;
    }
    if(tv->tv_nsec >= 1000000000) {
        // значение времени не нормализовано
        return ERR_ILLEGAL_ARGS;
    }
    rtime_to_set.tv_sec = tv->tv_sec;
    rtime_to_set.tv_nsec = tv->tv_nsec;
    rtime_flip_id = rtime_id ^ 1;
    return OK;
}

