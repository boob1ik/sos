#include <proc.h>
#include <mem\vm.h>

// args = (int tid)
void sc_thread_run (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно
    int tid = (int)thr->uregs->basic_regs[0];
    struct thread *t;

    thread_allocator_lock();
    t = get_thr(tid);
    if(t == NULL) {
        thread_allocator_unlock();
        thr->uregs->basic_regs[0] = ERR_ILLEGAL_ARGS;
        return;
    }

    thread_lock(t);
    if(t->state != STOPPED) {
        thread_allocator_unlock();
        thr->uregs->basic_regs[0] = ERR_BUSY;
        return;
    }
    thread_run(t);
    thread_unlock(t);
    thread_allocator_unlock();

    thr->uregs->basic_regs[0] = OK;
}
