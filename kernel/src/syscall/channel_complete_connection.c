#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>
#include <ipc/channel.h>

void sc_channel_complete_connection (struct thread *thr)
{
    int chid = (int)thr->uregs->basic_regs[CPU_REG_0];
    enum connection_cmd cmd = (enum connection_cmd)thr->uregs->basic_regs[CPU_REG_1];
    int result = (int)thr->uregs->basic_regs[CPU_REG_2];
    thr->uregs->basic_regs[CPU_REG_0] = channel_connection_complete(thr, chid, cmd, result);
}
