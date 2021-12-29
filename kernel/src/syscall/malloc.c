#include <proc.h>
#include <mem\vm.h>

// args = (int num, mem_attributes_t flags)
void sc_malloc (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно

    mem_attributes_t attr;
    *((size_t *) &attr) = thr->uregs->basic_regs[1];
    size_t pages = thr->uregs->basic_regs[0];
    thr->uregs->basic_regs[0] = (size_t)vm_alloc(thr->proc, pages, attr);
}
