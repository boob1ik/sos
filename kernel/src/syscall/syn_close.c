#include <proc.h>
#include <mem\vm.h>
#include <syn\syn.h>

// args = (int id);
void sc_syn_close(struct thread *thr) {
    // TODO какие-то проверки безопасности если нужно
    int id = thr->uregs->basic_regs[CPU_REG_0];
    int res = syn_close(id, thr->proc);
    thr->uregs->basic_regs[CPU_REG_0] = res; // return val
}
