#include <proc.h>
#include <mem\vm.h>
#include <interrupt.h>

// args = (int irq_id, int tid);
void sc_irq_hook(struct thread *thr) {
    // TODO какие-то проверки безопасности если нужно
    int irq_id = thr->uregs->basic_regs[CPU_REG_0];
    int tid = thr->uregs->basic_regs[CPU_REG_1];
    int res = ERR_ILLEGAL_ARGS;
    struct thread *handler = get_thr(tid);
    if( (handler != NULL) && (handler->state == STOPPED) ) {
        // запрошенный поток найден по номеру
        res = ERR_ACCESS_DENIED; // по умолчанию, если проверки ниже не прошли
        if( proc_equals(handler->proc, thr->proc) == OK ) {
            // процесс тот же
            struct interrupt_context *irqctx = interrupt_get_context(irq_id);
            if(irqctx != NULL) {
                // номер прерывания корректный, найден соответствующий контекст прерывания
                kobject_lock(&irqctx->lock);
                handler->irqctx = irqctx;
                res = interrupt_hook(irq_id, handler, INTERRUPT_THREAD);
                kobject_unlock(&irqctx->lock);
            }
        }
    }
    thr->uregs->basic_regs[CPU_REG_0] = res; // return val
}
