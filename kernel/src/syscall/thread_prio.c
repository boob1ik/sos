#include <proc.h>
#include <mem\vm.h>

// args = (int tid, int val)
void sc_thread_prio (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно

    asm volatile ("bkpt");
}
