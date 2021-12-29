#include <proc.h>
#include <mem\vm.h>

// args = (void *ptr)
void sc_mfree (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно
    void *adr = (void *)(thr->uregs->basic_regs[0]);
    thr->uregs->basic_regs[0] = vm_free(thr->proc, adr);
}
