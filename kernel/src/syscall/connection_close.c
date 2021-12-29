#include <os_types.h>
#include <thread.h>
#include <ipc/msg.h>
#include <ipc/connection.h>

void sc_connection_close (struct thread *thr)
{
    int id = (int)thr->uregs->basic_regs[CPU_REG_0];
    thr->uregs->basic_regs[CPU_REG_0] = close_connection(thr->proc, id);
}
