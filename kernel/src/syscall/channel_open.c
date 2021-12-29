#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>
#include <ipc/channel.h>

void sc_channel_open (struct thread *thr)
{
    channel_type_t type = (int)thr->uregs->basic_regs[CPU_REG_0];
    char *pathname = (char *)thr->uregs->basic_regs[CPU_REG_1];
    size_t size = thr->uregs->basic_regs[CPU_REG_2];
    unsigned long flags = thr->uregs->basic_regs[CPU_REG_3];
    thr->uregs->basic_regs[CPU_REG_0] = open_channel(thr, type, pathname, size, flags);
}
