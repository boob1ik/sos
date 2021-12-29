#include <arch.h>
#include <os.h>
#include "sched.h"
#include "mem\kmem.h"
#include "mem\vm.h"
#include "common\syshalt.h"
#include <common\log.h>
#include "syn\ksyn.h"

static struct thread *thread_tbl[THREAD_NUM + 1];
static int tid_cnt;
static kobject_lock_t tid_allocator_lock;
extern char __stack_svc_start__[];

void thread_allocator_init ()
{
    int i;
    for (i = 0; i < THREAD_NUM; i++)
        thread_tbl[i] = NULL;
    tid_cnt = 0;
    kobject_lock_init(&tid_allocator_lock);
}

inline void thread_allocator_lock() {
    kobject_lock(&tid_allocator_lock);
}

inline void thread_allocator_unlock() {
    kobject_unlock(&tid_allocator_lock);
}

inline void thread_lock(struct thread *thr) {
    kobject_lock(&thr->lock);
}

inline void thread_unlock(struct thread *thr) {
    kobject_unlock(&thr->lock);
}

int thread_init (struct thread_attr *attrs, struct process *p, struct thread **ret)
{
    if (tid_cnt > THREAD_NUM) {
        return ERR_THREAD_LIMIT;
//        syshalt(SYSHALT_TID_LIMIT); // число зарегистрированных потоков в системе превышает лимит
    }
    if( (attrs->ts < 1) || (attrs->ts > MAX_THREAD_TICKS) ) {
        return ERR_ILLEGAL_ARGS;
    }

    switch(attrs->type) {
    case THREAD_TYPE_JOINABLE:
    case THREAD_TYPE_DETACHED:
    case THREAD_TYPE_IRQ_HANDLER:
        break;
    default:
        return ERR_ILLEGAL_ARGS;
    }
    struct thread *thr = (struct thread *) kmalloc(sizeof(struct thread));
    memset(thr, 0, sizeof(struct thread));

    thr->type = attrs->type;
    // TODO Часть кода инициализации ниже перенести сюда когда не нужна работа под мьютексом

    attrs->stack_size += PAGE_SIZE;
    attrs->stack_size &= ~(PAGE_SIZE - 1);

    if (p->pid != IDLE_PID) { // !idle stacks
        thr->stack_size = attrs->stack_size;
        thr->stack = vm_alloc_stack(p->mmap, (attrs->stack_size >> PAGE_SIZE_SHIFT),
        false);
        thr->sys_stack_size = THREAD_SYSTEM_STACK_PAGES << PAGE_SIZE_SHIFT;
        thr->sys_stack = vm_alloc_stack(p->mmap, THREAD_SYSTEM_STACK_PAGES,
        true);
        if ((thr->stack == NULL) || (thr->sys_stack == NULL)) {
            syshalt(SYSHALT_KMEM_NO_MORE);
        }
    }

    thread_allocator_lock();
    int i = p->tid_alloc, j = i;
    while (1) {
        if (thread_tbl[j] == NULL) {
            thread_tbl[j] = thr;
            thr->tid = j;
            tid_cnt++;
            // p->tid_alloc имеем право изменить, т.к. используем глобальную блокировку tid_allocator_lock
            // и меняем только здесь
            p->tid_alloc++;
            if(p->tid_alloc >= THREAD_NUM) {
                p->tid_alloc = 1;
            }
            break;
        }
        j++;
        if (j >= THREAD_NUM)
            j = 1;
        if (j == i) {
            thread_allocator_unlock();
            // не найден свободный tid,
            syshalt(SYSHALT_TID_LIMIT); // число зарегистрированных потоков превышает лимит
        }
    }
    thread_allocator_lock();

    if (p->pid == IDLE_PID) { // state, idle stacks
        thr->state = READY;
        thr->stack_size = PAGE_SIZE;
        thr->stack = __stack_svc_start__ + PAGE_SIZE * thr->tid;
        thr->sys_stack_size = PAGE_SIZE;
        thr->sys_stack = thr->stack;
        if ((thr->stack == NULL) || (thr->sys_stack == NULL)) {
            syshalt(SYSHALT_KMEM_NO_MORE);
        }
    } else {
        thr->state = STOPPED;
    }
    thr->proc = p;
    thr->arg = attrs->arg;

//    struct thread *ct = cur_thr();
//    struct process *cp = cur_proc();
//    if ((ct == NULL) || (cp != p)) {
//        thr->prio = p->prio;
//    } else {
//        thr->prio = ct->prio;
//    }
    thr->prio = p->prio;
//    thr->nice = thr->prio;
    thr->nice = 0;

    thr->entry = attrs->entry;
    thr->time_slice = attrs->ts * CLOCK_TICK;
    // регистровый контекст будет инициализирован при активной карте памяти процесса в момент переключения
//    cpu_context_create(cpu_get_stack_pointer(thr->stack, thr->stack_size),
//            (isKernelPID(p->pid)) ? CPU_CONTEXT_KERNEL_PREEMPTED : CPU_CONTEXT_USER_PREEMPTED);

    kobject_lock_init(&thr->lock);
    // Объект процесса меняем только когда поток уже подготовлен полностью.
    // Для изменения состава/состояния процесса используем общую блокировку на процесс,
    // при этом получаем право изменять указатели process.inproc_list в
    // потоке, который уже есть в процессе (не используем блокировку на существующий поток)
    proc_lock(p);
    if (p->threads == NULL) {
        // создается первый поток процесса
        p->threads = thr;
        thr->substate = PROC_START;
    } else {
        // создается еще один поток процесса
        thr->inproc_list.prev = p->threads;
        p->threads->inproc_list.next = thr;
        p->threads = thr;
    }
    proc_unlock(p);

    log_info("+tid %i:%i entry=0x%08lx\n\r", p->pid, thr->tid, thr->entry);
    attrs->tid = thr->tid;
    *ret = thr;
    return OK;
}

int thread_set_arg(struct thread *t, void *arg) {
    if( (t->uregs != NULL) || (t->kregs != NULL) ) {
        return ERR;
    }
    t->arg = arg;
    return OK;
}

inline void thread_run (struct thread *thr)
{
    proc_active_threads_inc(thr->proc);
//    mutex_lock(thr->lock);
    thr->state = READY;
//    mutex_unlock(thr->lock);
    sched_lock();
    enqueue(thr);
    sched_unlock();
}

struct thread* get_thr (int tid)
{
    if ((tid >= 1) && (tid < THREAD_NUM)) {
        return thread_tbl[tid];
    }
    return NULL;
}

void thread_switch (struct thread *from, struct thread *to,
        struct cpu_context *retctx)
{
    // Нужно
    // - сохранить оставшийся контекст процессора предыдущего потока, если он есть
    // - поменять карту памяти если нужно
    // - переключиться и по требованию kreturn = 1 продолжить выполнение
    //      дальше за вызовом thread_context_switch после обратного переключения
    // Есть проблемки:
    // 1) Необходимо обеспечить обратный возврат в текущий сеанс ядра после переключений на др поток и обратно,
    // лучше в конец данной функции подстановкой точки возврата, для этого используем специальную
    // функцию сохранения текущего контекста процессора, указатель на контекст сохраняем в kregs.
    //  Таким образом у нас получается, что kregs будет указывать либо на прерванный контекст
    //  обработчика системного вызова сохраненный при входе в прерывание, либо на текущее состояние контекста
    //  обработчика системного вызова, сохраненное ниже. Механизм(функция) возврата не меняется,
    //  мы вернемся в правильные точки в этих двух случаях
    // 2) текущий стек должен работать при смене карты памяти,
    // поэтому используем глобальный стек ядра (для каждого ядра процессора свой);
    // на него имеем право переключиться после сохранения при необходимости текущего контекста для возврата, но
    // перед выполнением всех действий по переключению карты памяти и контекста на целевой поток
    // (стековая история глобального стека будет потеряна и не нужна дальше)

    if ((to == NULL) || ((from == NULL) && (retctx != NULL))) {
        syshalt(SYSHALT_THREAD_SWITCH_ERROR);
    }
    if (from == NULL) {
        vm_switch(NULL, to->proc->mmap);
    } else if (from != to) {
        struct process *pfrom = from->proc;
        if (from->state != DEAD) {
            // TODO здесь важное место алгоритма отложенного сохранения
            // контекста процессора при разных прерванных режимах.
            // На данный момент ядро не применяет команды сопроцессора
            // и сохранение идет в контекст пользовательского режима
            // в двух случаях :
            // в прерванном пользовательском режиме (CPU_CONTEXT_USER_PREEMPTED
            // и CPU_CONTEXT_KERNEL_SWITCH),
            // в прерванном режиме ядра (CPU_CONTEXT_KERNEL_PREEMPTED)
            cpu_context_save(from->uregs);
            if (retctx != NULL) {
                if(from->kregs == NULL) {
                    from->kregs = retctx;
                } else {
                    syshalt(SYSHALT_THREAD_SWITCH_ERROR);
                }
            }
        }
        if (from->proc != to->proc) {
            vm_switch(from->proc->mmap, to->proc->mmap);
        }

        if(pfrom->pid != IDLE_PID) {
            if(from->state == DEAD) {
                // Idle-потоки ядра никогда не должны перейти в состояние DEAD.
                // Остальные переходят в него после thread_exit или принудительного удаления thread_kill.
                if (proc_active_threads(pfrom) == 0) {
                    proc_finalize(pfrom, from);
                } else {
                    thread_finalize(from);
                }
            }

        }
/*
        if(pfrom->pid != IDLE_PID) {
            if( (from->state == DEAD) || (from->state == STOPPED) ) {
                // процесс мертвый, если в нем нет исполняющихся или блокированных потоков,
                // даже если есть включенные но не исполняющиеся обработчики прерываний
                // Алгоритм позволяет завершиться текущим работающим обработчикам прерываний и
                // удалить процесс синхронно в момент переключения контекста, что обеспечивает простоту и надежность
                proc_active_threads_dec(pfrom);
            }
            // Если можно удаляем из памяти поток и процесс
            // Процесс удаляется долго, поэтому это выполняется в системном контексте последнего DEAD-потока,
            // который переводится в состояние PROC_FINALIZING и остается там до завершения процедуры удаления процесса
            // Основной момент - поток в состоянии PROC_FINALIZING является рабочим и на него должны быть возможны
            // обратные переключения в системный контекст процедуры удаления процесса
            if (proc_active_threads(pfrom) == 0) {
                if(from->state == DEAD) {
                    proc_finalize(pfrom, from);
                }
            } else if(from->state == DEAD) {
                thread_finalize(from); // TODO
            }
        }
*/
    }

    // с этого момента карта памяти процесса, которому принадлежит поток,
    // становится активной, ядро имеет возможность выполнить код инициализации контекста
    // если поток новый, после чего выполняется переключение контекста процессора

    if ((to->kregs == NULL) && (to->uregs == NULL)) {
        to->uregs = cpu_context_create(
                cpu_get_stack_pointer(to->sys_stack, to->sys_stack_size),
                cpu_get_stack_pointer(to->stack, to->stack_size),
                (isKernelPID(to->proc->pid)) ?
                        CPU_CONTEXT_KERNEL_PREEMPTED :
                        CPU_CONTEXT_USER_PREEMPTED,
                to->entry,
                thread_exit_user_callback);
        if(to->proc->pid != IDLE_PID) {
            // Внимание! Из-за экономии памяти и специфики реализации idle-потока, который использует
            // "хитрый" механизм работы в вытесняемом контексте ядра и имеет общий системный и вытесняемый стек,
            // область TLS для idle не предусматривается и не должна портиться память в этом месте!!!
            cpu_context_set_tls(to->uregs, sizeof(struct thread_tls_user));
            struct thread_tls_user *tls = cpu_context_get_tls(to->uregs);
            tls->tid = to->tid;
            tls->pid = to->proc->pid;
            _REENT_INIT_PTR(&tls->reent);
        }
        switch(to->substate) {
        case NORMAL_START:
            cpu_context_set_thread_arg(to->uregs, to->arg);
            to->substate = NORMAL;
            break;
        case PROC_START:
            cpu_context_set_main_args(to->uregs, to->proc->arglen, to->proc->argv, to->proc->argtype);
            to->substate = NORMAL;
            break;
        default:
        case NORMAL:
            break;
        case PROC_FINALIZE:
            break;
        }
    }
    struct cpu_context *ctx = to->kregs;
    // выбираем контекст для переключения и удаляем его (ссылку на него)
    if (ctx == NULL) {
        // предыдущее состояние потока - исполнение основного кода в режиме пользователя
        // (или первый запуск потока)
        ctx = to->uregs;
        to->uregs = NULL;
    } else {
        // предыдущее состояние потока - исполнение системного вызова с вытеснением
        to->kregs = NULL;
    }
    cpu_context_switch(ctx);
}

void __attribute__ ((section (".user_text_common"))) thread_exit_user_callback ()
{
    register int result __asm__ ("r0");
    os_thread_exit(result);
}

void thread_exit (struct thread *ct)
{
    struct thread *t, *t2;
    struct process *cp = ct->proc;

    proc_active_threads_dec(cp);
    if (proc_active_threads(cp) == 0) {
        proc_prepare_finalize(cp, ct);
    }
    if(ct->irqctx != NULL) {
        // поток зарегистрирован как обработчик прерывания
        // для такого потока thread_exit должен приводить контекст
        // в исходное состояние перед его исполнением, поток должен остаться зарегистрированным
        // и привязанным к процессу. Удаление потока обработчика из памяти только по thread_kill
        // или thread_exit и только если поток не привязан к прерыванию
        interrupt_handle_end(ct->irqctx->id);
        ct->state = STOPPED;
        ct->time_sum = 0;
        ct->uregs = cpu_context_reset(
                cpu_get_stack_pointer(ct->sys_stack, ct->sys_stack_size),
                cpu_get_stack_pointer(ct->stack, ct->stack_size),
                (isKernelPID(ct->proc->pid)) ?
                        CPU_CONTEXT_KERNEL_PREEMPTED :
                        CPU_CONTEXT_USER_PREEMPTED,
                ct->entry,
                thread_exit_user_callback);
        if(cp->pid != IDLE_PID) {
            // Внимание! Из-за экономии памяти и специфики реализации idle-потока, который использует
            // "хитрый" механизм работы в вытесняемом контексте ядра и имеет общий системный и вытесняемый стек,
            // область TLS для idle не предусматривается и не должна портиться память в этом месте!!!
            cpu_context_set_tls(ct->uregs, sizeof(struct thread_tls_user));
//            struct thread_tls_user *tls = cpu_context_get_tls(ct->uregs);
//            tls->tid = ct->tid;
//            tls->pid = ct->proc->pid;
//            _REENT_INIT_PTR(&tls->reent); // TODO скорее всего для обработчиков прерываний не нужно
        }
//        to->kregs = NULL; // должно быть и так
        cpu_context_set_thread_arg(ct->uregs, ct->arg);
        return;
    }

    // Сначала удаляем из списка потоков
    proc_lock(cp);
    thread_lock(ct);
    if (cp->threads == ct) {
        cp->threads = ct->inproc_list.prev;
    }
    if (ct->inproc_list.prev != NULL) {
        ct->inproc_list.prev->inproc_list.next = ct->inproc_list.next;
    }
    if (ct->inproc_list.next != NULL) {
        ct->inproc_list.next->inproc_list.prev = ct->inproc_list.prev;
    }
    // разбиваем список ожидающих потоков и ставим их в очередь все здесь,
    // чтобы диспетчер потом имел возможность подхватить сразу один из них
    t = ct->last_joined;
    sched_lock();
    while(t != NULL) {
        t->state = READY;
        t->block.object.thr = NULL;
        enqueue(t);
        t2 = t->block_list.next;
        t->block_list.next = NULL;
        t->block_list.prev = NULL;
        t = t2;
    }
    sched_unlock();

    thread_unlock(ct);
    proc_unlock(cp);

    ct->state = DEAD;

    log_info("-tid %i:%i\n\r", ct->proc->pid, ct->tid);


    // vm_free_stack(ct->sys_stack); сейчас этого делать нельзя, это будет выполнено в
    // thread_switch чуть позже, где безопасно можно очистить память потока и, возможно,
    // процесса если поток в процессе был единственным
    // TODO нужно продумать с учетом других захваченных процессом ресурсов, кроме карты памяти

    // далее предполагается вызов переключения контекста и исполнение thread_switch с очисткой

    // вроде должен работать такой механизм удаления, нужно проверять
}

void thread_finalize (struct thread *thr) {
    thread_allocator_lock();
    thread_tbl[thr->tid] = NULL;
    tid_cnt--;
//    vm_free(thr->proc->mmap, thr->stack);
//    vm_free(thr->proc->mmap, thr->sys_stack);
    kfree(thr);
    thread_allocator_unlock();
}
