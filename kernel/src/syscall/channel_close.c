#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>
#include <ipc/channel.h>
#include <sched.h>

void sc_channel_close (struct thread *thr)
{
    int id = (int)thr->uregs->basic_regs[CPU_REG_0];
    thr->uregs->basic_regs[CPU_REG_0] = close_channel(thr->proc, id);
}
