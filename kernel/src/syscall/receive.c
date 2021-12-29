#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>

void sc_receive (struct thread *thr)
{
    int chid = (int)thr->uregs->basic_regs[CPU_REG_0];
    struct msg **m = (struct msg **)thr->uregs->basic_regs[CPU_REG_1];
    uint64_t timeout = thr->uregs->basic_regs[CPU_REG_2];
    timeout |= ((uint64_t)thr->uregs->basic_regs[CPU_REG_3]) << 32;
    thr->uregs->basic_regs[CPU_REG_0] = receive(thr, chid, m, timeout);
}
