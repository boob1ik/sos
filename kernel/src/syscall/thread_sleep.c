#include <proc.h>
#include <mem\vm.h>
#include <event.h>

// args = (uint64_t timeout)
void sc_thread_sleep (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно
    uint64_t ns = thr->uregs->basic_regs[CPU_REG_0];
    ns |= ((uint64_t)thr->uregs->basic_regs[CPU_REG_1]) << 32;
    ns += systime(); // вычисляем системное время наступления события
    thread_sleep_block(thr);
    thr->block_evt = kevent_insert(ns, thr); // добавляем событие в дерево
    thr->uregs->basic_regs[CPU_REG_0] = OK; // return val
}
