#include <os.h>
#include <string.h>
#include "plocal_mutex.h"
#include "plocal_sem.h"
/**
 * Тест синхронизации
 *
 *  1) Тестирование быстрого мьютекса строим на базе теста потоков.
 *  Каждый поток будет работать фиксированное число проходов по циклу,
 *  половина потоков будет инкрементировать защищаемую
 *  общую переменную, а половина - декриментировать в цикле.
 *  Если в конце теста полученное значение переменной равно начальному,
 *  значит синхронизация по быстрому мьютексу работает.
 *
 */


#define TEST_THREAD_COUNT        4
#define TEST_SLEEP_SEC           1
#define TEST_TIME_LIMIT_SEC      10
#define TEST_COUNT_LIMIT         1000000UL

int tid_tbl[TEST_THREAD_COUNT];
uint32_t test_cnt_[TEST_THREAD_COUNT];
kernel_time_t rt;

uint64_t test_var;
int test_var_synid;
syn_t synobj = {
    .type = MUTEX_TYPE_PLOCAL, //
    .pathname = NULL, //
    .cnt = 0, //
    .owner_tid = 0, //
    .limit = 0
};

void test_error() {
    while(1);
}

void test_success() {
    while(1);
}

void thread_test_mutex_plocal(uint32_t test_num) {
    kernel_time_t rtc;
    // Если делаем сначала паузу, то запускающий поток стартует максимально быстро все
    // порожденные тестовые, так как продолжает работать сразу после блокировки
    // на сон очередного порожденного потока, на который могло произойти переключение.
    // Время паузы можно выбирать относительно общего числа потоков и их времени запуска,
    // соотнесенного с размером кванта исходного потока.

    // Если указанные требования быстрого запуска соблюдать, то разброс тестовых значений
    // счетчиков не превысит небольшой диапазон, что позволяет сделать оценку
    // корректности работы диспетчера задач относительно потоков по результатам работы всех потоков.
    // Причем эту оценку можно делать именно на диапазон разброса значений с проверкой на минимальное
    // значение, то есть это будет работать на разных платформах с разным быстродействием
    os_thread_sleep(TEST_SLEEP_SEC * 1000000000ull);
    // эмуляция активной работы на TEST_TIME_LIMIT_SEC секунд
    do {
//        os_thread_yield();
        // если yield убрать то производительность возрастет раза в 2, но разброс большой
        // по времени работы потоков
//        os_time(OS_CLOCK_REALTIME, &rtc, NULL);
        test_cnt_[test_num]++;

//        os_syn_wait(test_var_synid, SYN_TIMEOUT_INFINITY);
        plocal_mutex_wait(test_var_synid, tid_tbl[test_num], &synobj, TIMEOUT_INFINITY);
        if(test_num & 1) {
            test_var--;
        } else {
            test_var++;
        }
        plocal_mutex_done(test_var_synid, tid_tbl[test_num], &synobj);
//        os_syn_done(test_var_synid);
//        os_thread_yield();

    } while(test_cnt_[test_num] < TEST_COUNT_LIMIT);
}

void thread_test_sem_plocal(uint32_t test_num) {
    kernel_time_t rtc;
    os_thread_sleep(TEST_SLEEP_SEC * 1000000000ull);
    do {
        test_cnt_[test_num]++;
        plocal_sem_wait(test_var_synid, tid_tbl[test_num], &synobj, TIMEOUT_INFINITY);
        if(test_num & 1) {
            test_var--;
        } else {
            test_var++;
        }
        plocal_sem_done(test_var_synid, tid_tbl[test_num], &synobj);

    } while(test_cnt_[test_num] < TEST_COUNT_LIMIT);
}

void thread_test_barrier(uint32_t test_num) {
    kernel_time_t rtc;
    do {
        os_syn_wait(test_var_synid, TIMEOUT_INFINITY);
        test_cnt_[test_num]++;

    } while(test_cnt_[test_num] < (TEST_COUNT_LIMIT >> 5));
}
int main(int argc, char *argv[])
{
    volatile int i = 0, tmp;
    kernel_time_t rtc;
    char *ptr;
    size_t len;
    int res;
    rt.tv_sec = 0;
    rt.tv_nsec = 0;

    // *******************************************************************************
    //          Проверка быстрых мьютексов
    // *******************************************************************************

    test_var_synid = os_syn_create(0, &synobj);
    if(test_var_synid <= 0) {
        test_error();
    }

    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        test_cnt_[i] = 0xffffffff;
    }
    test_var = 0;
    os_time(OS_CLOCK_REALTIME, NULL, &rt); // установка значения реального времени системы

    // инициализация потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        tid_tbl[i] = os_thread_create (thread_test_mutex_plocal, 0, 1, (void *)i);
        if(tid_tbl[i] < 0) {
            while(1);
        }
    }
    // запуск потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        os_thread_run (tid_tbl[i]);
    }
    // ожидание завершения потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        os_thread_join (tid_tbl[i]);
    }

    if(os_syn_delete(test_var_synid) != OK) {
        test_error();
    }
    if(os_syn_delete(test_var_synid) == OK) {
        test_error();
    }

    os_time(OS_CLOCK_REALTIME, &rtc, NULL);

    rtc.tv_sec -= rt.tv_sec;
    if(rtc.tv_nsec >= rt.tv_nsec) {
        rtc.tv_nsec -= rt.tv_nsec;
    } else {
        rtc.tv_sec--;
        rtc.tv_nsec = rt.tv_nsec - rtc.tv_nsec + 1;
    }

    // проверка результатов
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        if(test_cnt_[i] != TEST_COUNT_LIMIT) {
            test_error();
        }
    }

    if( test_var != 0 ) {
        test_error();
    }

    // *******************************************************************************
    //          Проверка быстрых семафоров
    // *******************************************************************************
    synobj.type = SEMAPHORE_TYPE_PLOCAL;
    synobj.limit = 1;
    test_var_synid = os_syn_create(0, &synobj);
    if(test_var_synid <= 0) {
        test_error();
    }

    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        test_cnt_[i] = 0xffffffff;
    }
    test_var = 0;
    os_time(OS_CLOCK_REALTIME, NULL, &rt); // установка значения реального времени системы

    // инициализация потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        tid_tbl[i] = os_thread_create (thread_test_sem_plocal, 0, 1, (void *)i);
        if(tid_tbl[i] < 0) {
            while(1);
        }
    }
    // запуск потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        os_thread_run (tid_tbl[i]);
    }
    // ожидание завершения потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        os_thread_join (tid_tbl[i]);
    }

    if(os_syn_delete(test_var_synid) != OK) {
        test_error();
    }
    if(os_syn_delete(test_var_synid) == OK) {
        test_error();
    }

    os_time(OS_CLOCK_REALTIME, &rtc, NULL);

    rtc.tv_sec -= rt.tv_sec;
    if(rtc.tv_nsec >= rt.tv_nsec) {
        rtc.tv_nsec -= rt.tv_nsec;
    } else {
        rtc.tv_sec--;
        rtc.tv_nsec = rt.tv_nsec - rtc.tv_nsec + 1;
    }

    // проверка результатов
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        if(test_cnt_[i] != TEST_COUNT_LIMIT) {
            test_error();
        }
    }

    if( test_var != 0 ) {
        test_error();
    }

    // *******************************************************************************
    //          Проверка барьеров
    // *******************************************************************************
    synobj.type = BARRIER_TYPE_PSHARED;
    synobj.limit = 4;
    test_var_synid = os_syn_create(0, &synobj);
    if(test_var_synid <= 0) {
        test_error();
    }

    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        test_cnt_[i] = 0xffffffff;
    }
    test_var = 0;
    os_time(OS_CLOCK_REALTIME, NULL, &rt); // установка значения реального времени системы

    // инициализация потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        tid_tbl[i] = os_thread_create (thread_test_barrier, 0, 1, (void *)i);
        if(tid_tbl[i] < 0) {
            while(1);
        }
    }
    // запуск потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        os_thread_run (tid_tbl[i]);
    }
    // ожидание завершения потоков
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        os_thread_join (tid_tbl[i]);
    }

    if(os_syn_delete(test_var_synid) != OK) {
        test_error();
    }
    if(os_syn_delete(test_var_synid) == OK) {
        test_error();
    }

    os_time(OS_CLOCK_REALTIME, &rtc, NULL);

    rtc.tv_sec -= rt.tv_sec;
    if(rtc.tv_nsec >= rt.tv_nsec) {
        rtc.tv_nsec -= rt.tv_nsec;
    } else {
        rtc.tv_sec--;
        rtc.tv_nsec = rt.tv_nsec - rtc.tv_nsec + 1;
    }

    // проверка результатов
    for(i = 0; i < TEST_THREAD_COUNT; i++) {
        if(test_cnt_[i] != (TEST_COUNT_LIMIT >> 5)) {
            test_error();
        }
    }
//    test_success();

}
