#include <thread.h>

// args = ()
void sc_thread_yield (struct thread *thr)
{
    thr->time_sum = 0;
    // переход с RUNNING на READY текущего потока приведет к перепостановке потока в очередь
    // в момент вызова диспетчера с переключением контекста
    thr->state = READY;
    thr->uregs->basic_regs[CPU_REG_0] = OK; // return val
}
