#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>

void sc_send (struct thread *thr)
{
    int conid = (int)thr->uregs->basic_regs[CPU_REG_0];
    struct msg *m = (struct msg *)thr->uregs->basic_regs[CPU_REG_1];
    uint64_t timeout = thr->uregs->basic_regs[CPU_REG_2];
    timeout |= ((uint64_t)thr->uregs->basic_regs[CPU_REG_3]) << 32;
    int flags = (int)thr->uregs->basic_regs[CPU_REG_4];
    thr->uregs->basic_regs[CPU_REG_0] = send(thr, conid, m, timeout, flags);
}
