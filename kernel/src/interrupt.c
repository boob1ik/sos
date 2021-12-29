#define SCHED_CURRENT

#include <arch.h>
#include "sched.h"
#include "interrupt.h"
#include <common\log.h>


/**
 * Предполагается, что обработчик вызывается сразу после сохранения контекста процессора или
 * его минимальной части без сопроцессоров
 * Вытеснение должно быть выключено, оно работает только в режимах User и
 * SVC при длительном системном вызове
 */
void interrupt_handler (struct cpu_context *ctx)
{
    const struct interrupt_context *info = interrupt_handle_begin();
    struct thread *handler, *prev;
    if (info != NULL) {
        prev = cur_thr();
        // всегда сохраняем ссылку на контекст процессора в потоке, если он исполнялся (в USR или SVC)
        // ссылка может потребоваться в том числе и в функции ядра
        if(prev != NULL) {
            if(prev->uregs != NULL) {
                // предыдущий(текущий) выполнялся в SVC режиме, то есть выполнял длинный системный вызов
                // поэтому контекст является контекстом ядра
                prev->kregs = ctx;
                ctx->type = CPU_CONTEXT_KERNEL_PREEMPTED;
            } else {
                prev->uregs = ctx;
                ctx->type = CPU_CONTEXT_USER_PREEMPTED;
            }
        }
        switch (info->flags.type) {
        case INTERRUPT_KERNEL_FUNC:
            ((void (*) (const struct interrupt_context *info)) (info->handler))(info);
            break;
        case INTERRUPT_THREAD:

            // даем возможность сохранить остаток контекста процессора предыдущего потока,
            // так как будем переключаться на другой поток
//            cpu_context_save(ctx);
            handler = (struct thread *) (info->handler);
            if(handler != NULL) {
                if(proc_active_threads(handler->proc) == 0) {
                    // Если не стало активных потоков в процессе(включая обработчики прерывания, их не отделяем от
                    // других потоков), то больше новые события не обрабатываем, так как будет выполняться процедура
                    // завершения процесса. Чтобы исключить зацикливание событий по текущему вектору, необходимо его
                    // блокировать здесь
                    interrupt_disable(info->id);
                    break;
                }
                thread_run(handler);
                sched_switch(SCHED_SWITCH_NO_RETURN);
//                handler->state = READY;
//                sched_enqueue_switch(handler, SCHED_SWITCH_NO_RETURN);
            }
            // сюда не вернемся, выполняется переключение
            // на поток в порядке очереди исполнения (не обязательно на поток
            // обработки полученного сейчас прерывания, если не используется
            // вытеснение только более высокоприоритетными прерываниями
            // с аппаратной поддержкой приоритетов)
            // метод interrupt_handle_end должен быть выполнен после завершения
            // потока обработки прерывания
            break;
        }
        // если сюда пришли, значит ссылка на контекст больше не нужна, нужно ее удалить
        if(prev != NULL) {
            if(prev->kregs != NULL) {
                prev->kregs = NULL;
            } else {
                prev->uregs = NULL;
            }
        }
        interrupt_handle_end(info->id);
    }
}
