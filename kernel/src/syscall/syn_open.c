#include <proc.h>
#include <mem\vm.h>
#include <syn\syn.h>
#include <string.h>

// args = (int id, char *pathname, uint64_t timeout);
void sc_syn_open(struct thread *thr) {
    // TODO какие-то проверки безопасности если нужно
    char *s = (char *)thr->uregs->basic_regs[CPU_REG_0];
    enum syn_type type = (enum syn_type)thr->uregs->basic_regs[CPU_REG_1];
    int res = syn_open(s, type, thr->proc);
    thr->uregs->basic_regs[CPU_REG_0] = res; // return val
}
