#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>
#include <ipc/channel.h>

void sc_channel_wait_connection (struct thread *thr)
{
    int chid = (int)thr->uregs->basic_regs[CPU_REG_0];
    struct connection_info *info = (struct connection_info *)thr->uregs->basic_regs[CPU_REG_1];
    uint64_t timeout = thr->uregs->basic_regs[CPU_REG_2];
    timeout |= ((uint64_t)thr->uregs->basic_regs[CPU_REG_3]) << 32;
    thr->uregs->basic_regs[CPU_REG_0] = channel_connection_wait (thr, chid, info, timeout);
}
