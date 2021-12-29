#include <proc.h>
#include <mem\vm.h>
#include <syn\syn.h>

// args = (int id, uint64_t timeout);
void sc_syn_wait(struct thread *thr) {
    // TODO какие-то проверки безопасности если нужно
    int id = thr->uregs->basic_regs[CPU_REG_0];
    uint64_t ns = thr->uregs->basic_regs[CPU_REG_2];
    ns |= ((uint64_t)thr->uregs->basic_regs[CPU_REG_3]) << 32;

    int res = syn_wait(id, thr, ns);
    thr->uregs->basic_regs[CPU_REG_0] = res; // return val
}
