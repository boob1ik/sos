#include <proc.h>
#include <mem\vm.h>

// args = (int tid)
void sc_thread_stop (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно

    thread_stop_current(thr);
    thr->uregs->basic_regs[0] = OK;
}
