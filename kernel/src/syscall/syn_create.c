#include <proc.h>
#include <mem\vm.h>
#include <syn\syn.h>

// args = (int id, syn_t *s);
void sc_syn_create(struct thread *thr) {
    // TODO какие-то проверки безопасности если нужно
    syn_t *s = (syn_t *)thr->uregs->basic_regs[CPU_REG_0];
    int res = syn_create(s, thr->proc);
    thr->uregs->basic_regs[CPU_REG_0] = res; // return val
}
