#include <proc.h>
#include <mem\vm.h>

// args = (size_t address, int num, mem_attributes_t flags)
void sc_mmap (struct thread *thr)
{
    // TODO какие-то проверки безопасности если нужно
    mem_attributes_t attr = *(mem_attributes_t *)&thr->uregs->basic_regs[2];
    void *adr = (void *)(thr->uregs->basic_regs[0]);
    size_t pages = (size_t)(thr->uregs->basic_regs[1]);
    thr->uregs->basic_regs[0] = vm_map(thr->proc, adr, pages, attr);
}
