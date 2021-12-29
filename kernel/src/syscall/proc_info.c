#include <proc.h>
#include <mem\vm.h>

// args = (int pid, struct proc_info *info)
void sc_proc_info (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно

    asm volatile ("bkpt");
}
