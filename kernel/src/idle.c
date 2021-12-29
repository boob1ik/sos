#include <arch.h>
#include "idle.h"
#include "sched.h"

void init_idle() {
    proc_init_idle();

}

//extern uint64_t inc(uint64_t v);

void idle_time_test() {

    uint64_t t = systime(), tmp, test = 0;
    kernel_time_t rt, tstamp;
//    rt.tv_sec = 100;
//    rt.tv_nsec = 0;
//    time_set(OS_CLOCK_REALTIME, &rt);
    while(1) {
        cpu_wait_energy_save();
        //tmp = inc(test);
        if(test + 1 != tmp) {
            tmp = 0;
        }
        test = tmp;

        tmp = systime();
        if(tmp - t >= 1000000000) {
            time_get(OS_CLOCK_REALTIME, &rt);
            t += 1000000000;                // breakpoint here to test
        }
    }
}

void idle_generic() {
    uint64_t fake = 0;
    while(1) {
        cpu_wait_energy_save();
        fake++;
    }
}

/**
    Главное правило для idle потоков - они не должны блокироваться по
    каким-либо синхронизациям на мьютексах и т.п. или на обмене сообщениями.
    Следствие - не должны выполняться вызовы которые могут перевести поток в блокировку.
    Основное назначение idle - перевод ядра процессора в энергосберегающий режим или
    простой при отсутствии потоков для выполнения в очереди готовых.
    Так как потоки idle работают в карте памяти ядра и режиме System, то
    можем применять прямые функции ядра вместо системных вызовов,
    но осторожно и с пониманием динамики процесса.
 */
void idle() {
    idle_time_test();
//    idle_generic();
}
