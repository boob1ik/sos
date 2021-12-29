#include <proc.h>
#include <mem\vm.h>

// args = (void *entry, int stack_size, int ts, void *arg)
void sc_thread_create (struct thread *thr)
{
    struct thread_attr *attrs = (struct thread_attr *)thr->uregs->basic_regs[0];
    struct thread *newthr;
    int res = ERR_ILLEGAL_ARGS;
    // TODO какие-то проверки безопасности если нужно

    if( (attrs->ts < 1) || (attrs->ts > MAX_THREAD_TICKS) ) {
        thr->uregs->basic_regs[CPU_REG_0] = res;
        return;
    }

    res = thread_init(attrs, thr->proc, &newthr);
    if(res != OK) {
        thr->uregs->basic_regs[CPU_REG_0] = res;
        return;
    }
    thr->uregs->basic_regs[CPU_REG_0] = newthr->tid;
}
