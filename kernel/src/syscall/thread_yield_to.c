#include <thread.h>

// args = (int tid)
void sc_thread_yield_to(struct thread *thr)
{
    asm volatile ("bkpt");
}
