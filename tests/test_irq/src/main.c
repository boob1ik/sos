#include <os.h>
#include <string.h>
#include "plocal_mutex.h"
#include "plocal_sem.h"

#include "epit.h"

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
        plocal_mutex_wait(test_var_synid, tid_tbl[test_num], &synobj, SYN_TIMEOUT_INFINITY);
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
        plocal_sem_wait(test_var_synid, tid_tbl[test_num], &synobj, SYN_TIMEOUT_INFINITY);
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
        os_syn_wait(test_var_synid, SYN_TIMEOUT_INFINITY);
        test_cnt_[test_num]++;

    } while(test_cnt_[test_num] < (TEST_COUNT_LIMIT >> 5));
}
mem_attributes_t mattr = {
    .shared = MEM_SHARED_OFF, //
    .exec = MEM_EXEC_NEVER, //
    .type = MEM_TYPE_STRONGLY_ORDERED, //
    .inner_cached = MEM_CACHED_OFF, //
    .outer_cached = MEM_CACHED_OFF, //
    .process_access = MEM_ACCESS_RW, //
    .os_access = MEM_ACCESS_RW, //
    .security = MEM_SECURITY_OFF };
int main(int argc, char *argv[])
{
    volatile int i = 0, tmp;
    kernel_time_t rtc;
    char *ptr;
    size_t len;
    int res;
    rt.tv_sec = 0;
    rt.tv_nsec = 0;
    os_time(OS_CLOCK_REALTIME, NULL, &rt); // установка значения реального времени системы

    timer_init(1000000000);
    timer_enable();

    test_success();
}
