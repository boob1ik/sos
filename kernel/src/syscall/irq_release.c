#include <proc.h>
#include <mem\vm.h>
#include <interrupt.h>

// args = (int irq_id);
void sc_irq_release(struct thread *thr) {
    // TODO какие-то проверки безопасности если нужно
    int irq_id = thr->uregs->basic_regs[CPU_REG_0];
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
                // только процесс-владелец прерывания может его освободить
                // но... нужно гарантировать корректность этой операции
                // Для этого нужно запретить прерывание и обязательно
                // дождаться завершения прохода обработчика если он работал,
                // т.к. по наличию ссылки handler->irqctx выполняется
                // заверешение потока либо как обычно с освобождением ресурсов
                // либо как обработчика прерывания TODO

                interrupt_disable(irq_id);
                if(handler->state != STOPPED) {
                    // пока вернем ошибку потом можно сделать ожидание
                    // если будет желание
                    res = ERR_BUSY;
                } else {
                    res = interrupt_release(irq_id);
                    handler->irqctx = NULL;
                }
            } else {
                res = ERR_ACCESS_DENIED;
            }
        }
        kobject_unlock(&irqctx->lock);
    }
    thr->uregs->basic_regs[CPU_REG_0] = res; // return val
}
