/**
 Основная концепция.

 Планировщик управляет распределением времени работы потоков всех процессов в системе,
 включая IDLE-процесс с отдельными потоками для каждого ядра в случае их бездействия.
 Применяется алгоритм планирования, основанный на очередях приоритетов потоков.
 В системе доступно PRIO_NUM приоритетов и соответствующих им очередей.
 Очередь IDLE_PRIO (самая низкая) используется только процессом - "бездействие".

 Потоки всегда вытесняемые, в том числе и в ядре.
 Существует только 4 причины переключения потока:
 - блокировка по системному вызову (sys_call)
 - приоритетное вытеснение (interrupt)
 - выработка кванта (clock)
 - добровольная (yield)

 Имеется три политики планирования внутри очереди одного приоритета:
 - Очередь
 алгоритм диспетчеризации, при котором все процессы выбираются из очереди в порядке поступления
 и отрабатывают до завершения
 - Циклический (карусельный) = очередь + кванты времени
 алгоритм диспетчеризации, при котором все процессы выбираются из очереди в фиксированном циклическом порядке
 на выделенный квант времени
 - Специальная
 Большой секрет )

 */

#include <arch.h>
#include "sched.h"
#include "interrupt.h"
#include "common\utils.h"
#include "syn\ksyn.h"
#include "event.h"

extern char __stack_svc_end__[];
static void *kernel_global_stack[NUM_CORE];
static void *kernel_global_sp[NUM_CORE];

// очередь готовых на выполнение потоков по приоритетам
// TODO - переделать из массива на список
static struct rdy_queue {
    struct thread *head;
    struct thread *tail;
} rdy[PRIO_NUM];

// текущий выполняемый поток (на каждом ядре)
static struct thread* run_thr[NUM_CORE];
static uint64_t run_thr_tstamp[NUM_CORE];

// потоки бездействия
static struct thread* idle_thr[NUM_CORE];

static kobject_lock_t schedlock;

struct thread* cur_thr ()
{
    return run_thr[cpu_get_core_id()];
}

struct process* cur_proc ()
{
    return (run_thr[cpu_get_core_id()] == NULL) ?
            NULL : run_thr[cpu_get_core_id()]->proc;
}

//static int yield_q; // очередь потом ожидающих "разъелденья"
//static int sleep_q; // очередь спящих потоков

static void sched_tick ();

static void init_gstacks() {
    for(int i = 0; i < NUM_CORE; i++) {
        kernel_global_stack[i] = __stack_svc_end__ - PAGE_SIZE * 2 - i * PAGE_SIZE;
        kernel_global_sp[i] = kernel_global_stack[i] + PAGE_SIZE;
    }
/*
    for(int i = 0; i < NUM_CORE; i++) {
        kernel_global_stack[i] = vm_alloc_stack(kmap, THREAD_SYSTEM_STACK_PAGES, true);
        if(kernel_global_stack[i] == NULL) {
            syshalt(SYSHALT_KMEM_NO_MORE);
        }
        kernel_global_sp[i] = cpu_get_stack_pointer(kernel_global_stack[i], THREAD_SYSTEM_STACK_PAGES << PAGE_SIZE_SHIFT);
    }
*/
}

static void init_queues ()
{
    for (int i = 0; i < PRIO_NUM; i++)
        rdy[i].head = rdy[i].tail = NULL;
}

static void init_rdy ()
{
    for (int i = 0; i < NUM_CORE; i++)
        run_thr[i] = NULL;
}

static void init_proc ()
{
    proc_allocator_init();
}

static void init_thread ()
{
    thread_allocator_init();
}

void sched_init ()
{
    init_gstacks();
    init_queues();
    init_rdy();
    init_proc();
    init_thread();

    proc_init_idle();
    proc_init_kernel();
    struct thread *t = idle_proc->threads;
    int i = NUM_CORE - 1;
    while(t != NULL) {
        idle_thr[i] = t;
        t = t->inproc_list.prev;
        i--;
    }
    timer_init(CLOCK_TICK);
    interrupt_hook(SCHEDULER_IRQ_ID, sched_tick, INTERRUPT_KERNEL_FUNC);
    interrupt_set_assert_type(SCHEDULER_IRQ_ID, IRQ_ASSERT_EDGE_RISING);
    interrupt_set_priority(SCHEDULER_IRQ_ID, KPRIO_MAX);
    interrupt_set_cpu(SCHEDULER_IRQ_ID, CPU_0, 1);

#ifdef BUILD_SMP
    int cpu = CPU_1;
    for(int i = SCHEDULER_IRQ_SECONDARY_BASE; i < SCHEDULER_IRQ_SECONDARY_BASE + NUM_CORE; i++) {
        interrupt_hook(i, sched_tick, INTERRUPT_KERNEL_FUNC);
        interrupt_set_assert_type(i, IRQ_ASSERT_EDGE_RISING);
        interrupt_set_priority(i, KPRIO_MAX);
        interrupt_set_cpu(i, cpu++, 1);
    }
#endif

    interrupt_enable(SCHEDULER_IRQ_ID);
}

void sched_start ()
{
    timer_enable();
    sched_switch(SCHED_SWITCH_NO_RETURN);
    // сюда при нормальной работе никогда не попадем
}

void sched_lock ()
{
    kobject_lock(&schedlock);
}

void sched_unlock ()
{
    kobject_unlock(&schedlock);
}

/* Эта функция определяет политику планирования.
 Она вызывается, чтобы определить очередь, в которую нужно поставить поток. */
static void sched (struct thread *thr, int *queue, int *front)
{
    static struct thread *prev = 0;
    int expire = thr->time_sum > thr->time_slice;
    int penalty = 0;

    /* Проверка наличия неизрасходованного времени у процесса. При его отсутствии выделение
     еще одного кванта и, возможно, повышение приоритета. Приоритет потоков, использующих
     несколько квантов подряд, понижается, чтобы избежать бесконечного зацикливания потоков
     высокоприоритетных процессов (системных менеджеров и драйверов). */
    if (expire) {
        thr->time_sum = 0;
        //penalty += (prev == thr) ? -1 : 1;
        if (prev == thr) {
            penalty += -1;
            //kpintf("SCHED: bad thread %d", thr->id);
        } else {
            prev = thr;
            penalty += 1;
        }
    }

    if (penalty || thr->nice) {
        thr->prio += penalty + thr->nice;
        if (thr->prio < PRIO_MAX)
            thr->prio = PRIO_MAX;
        else if (thr->prio > PRIO_MIN)
            thr->prio = PRIO_MIN;
    }

    *queue = thr->prio;
    *front = expire;
}

/* enqueue, dequeue и pick реализуют механизм планирования */

/* Добавление потока в очередь планирования */
void enqueue (struct thread *thr)
{
    int q;
    int front;
    sched(thr, &q, &front);

    ++thr->stat.run;

    if (rdy[q].head == NULL) {                // пустая очередь
        rdy[q].head = rdy[q].tail = thr;
        thr->ready_list.next = NULL;
    } else if (front) {                       // в начало
        thr->ready_list.next = rdy[q].head;
        rdy[q].head = thr;
    } else {                                  // в конец
        rdy[q].tail->ready_list.next = thr;
        rdy[q].tail = thr;
        thr->ready_list.next = NULL;
    }
}

/* Удаление потока из планирования */
void dequeue (struct thread *thr)
{
    int q = thr->prio;
    if (rdy[q].head == thr && rdy[q].tail == thr) {
        rdy[q].head = rdy[q].tail = NULL;
    } else if (rdy[q].head == thr) {
        rdy[q].head = thr->ready_list.next;
    } else if (rdy[q].tail == thr) {
        rdy[q].tail = thr->ready_list.prev;
    } else {
        thr->ready_list.prev->ready_list.next = thr->ready_list.next;
    }
}

/* Выбор потока для выполнения. */
static struct thread* pick ()
{
    for (int i = 0; i < PRIO_NUM; i++) {
        if (rdy[i].head)
            return rdy[i].head;
    }
    // нет готовых
    return NULL;
}

/**
 *  Переключение потоков.
 *  Внимание! Пока что функция может вызываться только при отключенных прерываниях(без вытеснения)
*/
void sched_switch (enum sched_switch_mode mode)
{
    int core = cpu_get_core_id();
    uint64_t timestamp;
    struct thread *current, *pending = NULL, *encoming;

    sched_lock();

    // Принимаем решение на какой поток переключиться (выполняем диспетчеризацию) в три этапа
    // 1) Проверка на появление новых событий по блокированным потокам, их разблокировка
    //      и постановка в очередь в соответствии с выбранной политикой планирования
    // 2) Проверка текущего потока на истечение кванта, перепостановка его в очередь в
    //      случае необходимости
    // 3) Выборка следующего потока по плану, проверка на вытеснение, переключение на новый
    //      или старый поток

    // Необходимо обрабатывать наступившие события по времени.
    // TODO
    // В текущем варианте будем делать это сразу по всем наступившим.
    // Возможно также прореживание во времени с задержкой, чтобы не перегружать
    // время выполнения sched_switch, то есть обрабатывать по 1 или нескольким событиям
    // за один вызов sched_switch, что гарантирует его конечное маленькое время выполнения

    kevent_store_lock();
    while( (encoming = kevent_fetch()) != NULL ) {
        switch(encoming->block.type) {
        case SYN_MUTEX:
        case SYN_SEMAPHORE:
        case SYN_BARRIER:
            thread_syn_cancel(encoming);
            enqueue(encoming);
            break;
        case SLEEP:
            thread_sleep_unblock(encoming);
            enqueue(encoming);
            break;
        default:
            break;
        }
        encoming->block_evt = NULL;
    }
    kevent_store_unlock();

    current = run_thr[core];
    // Примечание.
    // Далее обнуление run_thr[core] используем как признак смены текущего потока current
    timestamp = systime();
    if( (current != NULL) && (current != idle_thr[core]) ) {
        // idle тут не обрабатываем, он всегда должен работать если других нет
        // здесь принимаем решение о судьбе текущего потока
        current->time_sum += timestamp - run_thr_tstamp[core];
        if( current->state == RUNNING ) {
            // текущий поток не поменял состояние,
            // необходимо учесть его рабочее время в кванте и,
            // если он исчерпал его в активном режиме, принять решение по его дальнейшей судьбе
            if(current->time_sum > current->time_slice) {
                // сбрасываем работу текущего потока
                // и снова ставим в очередь планирования,
                // планировщик решит что делать с захватчиком процессорного времени
                current->time_sum = 0;
                run_thr[core] = NULL;
                current->state = READY;
                enqueue(current);
            } else {
/*
                // нужно переключиться обратно на текущий поток
                run_thr_tstamp[core] = timestamp;
                sched_unlock();
                call_thread_switch_s(current, current, (mode == SCHED_SWITCH_SAVE_AND_RET), kernel_global_sp[core]);
                return;
*/
            }
            /*
             struct thread *thr = run_thr[cpu_get_core_id()];
             if (!thr->time_left--) {
             thr->stat.run += thr->time_slice;
             dequeue(thr);
             enqueue(thr);
             sched_switch();
             }

             if (thr->time_grant) {
             if (!thr->time_grant->time_left--) {
             thr->time_grant = NULL;
             dequeue(thr->time_grant);
             enqueue(thr->time_grant);
             sched_switch();
             }
             }
             else if (!thr->time_left--) {
             thr->stat.run += thr->time_slice;
             dequeue(thr);
             enqueue(thr);
             sched_switch();
             }
             */
        } else {
            // состояние текущего потока поменялось с RUNNING на другое
            run_thr[core] = NULL;
            if(current->state == READY) {
                // TODO возможно ли что ядро перевело поток на READY сразу с RUNNING ?
                // в любом случае корректным будет постановка в очередь планирования
                enqueue(current);
            }
        }
    }
    pending = pick();
    if( pending != NULL ) {
        if( (run_thr[core] == NULL) || (thread_preemptable(run_thr[core], pending)) ) {
            dequeue(pending);
            pending->state = RUNNING;
            if(run_thr[core] != NULL) {
                // вытеснение более приоритетным, перепостановка в очередь вытесняемого,
                // квант текущего не сбрасываем
                current->state = READY;
                if(current != idle_thr[core]) {
                    enqueue(current);
                }
            }
            run_thr[core] = pending;
        } else {
            pending = run_thr[core];//current; // поток сохранился (idle или любой другой)
        }
    } else {
        if(run_thr[core] == NULL) {
            pending = idle_thr[core];
            pending->state = RUNNING;
            run_thr[core] = pending;
        } else {
            pending = current; // поток сохранился (idle или любой другой)
        }
    }
    run_thr_tstamp[core] = timestamp;
    sched_unlock();
    // коррекция приоритета вытеснения в контроллере прерываний
//    interrupt_set_env_priority(pending->prio);
    call_thread_switch_s(current, pending, (mode == SCHED_SWITCH_SAVE_AND_RET), kernel_global_sp[core]);
}

void sched_enqueue_switch (struct thread *thr, enum sched_switch_mode mode)
{
    sched_lock();
    enqueue(thr);
    sched_switch(mode);
}

static void sched_tick (const struct interrupt_context *info)
{
    timer_event();
    interrupt_handle_end(info->id);         // завершаем обязательно прерывание
    sched_switch (SCHED_SWITCH_NO_RETURN);  // переключаемся обратно или на другой поток
}

