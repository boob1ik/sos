#include <proc.h>
#include <mem\vm.h>
#include <interrupt.h>
#include <syn\ksyn.h>

// args = (int irq_id, irq_ctrl_t ctrl);
void sc_irq_ctrl(struct thread *thr) {
    // TODO какие-то проверки безопасности если нужно
    int irq_id = thr->uregs->basic_regs[CPU_REG_0];
    irq_ctrl_t ctrl;
    *((size_t *) &ctrl) = thr->uregs->basic_regs[CPU_REG_1];
    int res = ERR_ACCESS_DENIED;
    struct interrupt_context *irqctx = interrupt_get_context(irq_id);
    if(irqctx != NULL) {
        // номер прерывания корректный, найден соответствующий контекст прерывания
        kobject_lock(&irqctx->lock);
        struct thread *handler = (struct thread *)irqctx->handler;
        res = ERR_DEAD;
        if(handler != NULL) {
            // есть обработчик на прерывании
            if( proc_equals(handler->proc, thr->proc) == OK ) {
                // только процесс-владелец прерывания может им управлять
                interrupt_disable(irq_id);
                interrupt_set_assert_type(irq_id, ctrl.assert_type);
                interrupt_set_priority(irq_id, handler->prio);
                int i = 0;
#ifdef BUILD_SMP
                int numcore = cpu_get_core_id();
                for(; i < NUM_CORE; i++) {
                   interrupt_set_cpu(irq_id, i, 0);
                   if(i == numcore) {
#endif
                        interrupt_set_cpu(irq_id, i, 1);
#ifdef BUILD_SMP
                   }
                }
#endif
                if(ctrl.enable) {
                    interrupt_enable(irq_id);
                }
                res = OK;
            } else {
                res = ERR_ACCESS_DENIED;
            }
        }
        kobject_unlock(&irqctx->lock);
    }
    thr->uregs->basic_regs[CPU_REG_0] = res; // return val
}
