#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>
#include <ipc/connection.h>

void sc_connection_open (struct thread *thr)
{
    char *pathname = (char *)thr->uregs->basic_regs[CPU_REG_0];
    int chid_reply = (int)(thr->uregs->basic_regs[CPU_REG_1]);
    uint64_t timeout = thr->uregs->basic_regs[CPU_REG_2];
    timeout |= ((uint64_t)thr->uregs->basic_regs[CPU_REG_3]) << 32;
    thr->uregs->basic_regs[CPU_REG_0] = (size_t)open_connection(thr, pathname, timeout, chid_reply);
}
