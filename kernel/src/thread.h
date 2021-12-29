#ifndef THREAD_H
#define THREAD_H

#include <arch.h>
#include <string.h>
#include "syn\ksyn.h"
#include "syn\sem.h"
#include "syn\mutex.h"
#include "syn\barrier.h"
#include "event.h"
#include "ipc\connection.h"

struct thread {
    kobject_lock_t lock;   // TODO пока делается допущенние что только одноядерный режим поэтому блокировка считай нигде не используется

    int tid;
    void *entry;
    struct process *proc;                   //!< процесс-владелец
    struct thread *parent;
    struct interrupt_context *irqctx;       //!< если поток назначен как обработчик прерывания
    void *arg;                              //!< аргумент для запуска потока
    thread_type_t type;

    struct cpu_context *uregs;
    struct cpu_context *kregs;
    void *stack;
    size_t stack_size;
    void *sys_stack;
    size_t sys_stack_size;

    int prio;                               //!< текущий (динамический) приоритет потока
    int nice;
    uint64_t time_slice;
    uint64_t time_sum;
    struct thread *time_grant;              //!< грант кванта вызовом yield_to

    struct {
        int core :4;                        //!< ядро на котором выполняется или выполнялся поток
        int running :1;
    };

    enum {
        READY,
        RUNNING,
        BLOCKED,
        STOPPED,
        DEAD
    } state;

    enum {
        NORMAL_START,
        PROC_START,
        NORMAL,
        PROC_FINALIZE
    } substate;

    struct {
        enum {
            SLEEP,
            JOIN,
            SYN_MUTEX,
            SYN_SEMAPHORE,
            SYN_BARRIER,
            CONNECT,
            WAIT_CONNECT,
            SEND,
            RECEIVE,
            WFI
        } type;
        union {
            void *ref;
            mutex_t *mutex;
            semaphore_t *sem;
            barrier_t *brr;
            struct thread *thr;
            struct {
                size_t size;
                struct connection *connection;
            } send;
            struct connection *connection;
            struct channel *channel;
        } object;
    } block;

    struct kevent *block_evt; //!< событие, == NULL - сработало или не было установлено; != NULL было установлено

    struct thread *last_joined;

    //!< В один момент времени поток либо находится в списке готовых к выполнению потоков,
    //!< либо блокирован и, возможно, определен объект блокировки для которого строится список.
    //!< Поэтому достаточно иметь только один комплект списочных указателей
    union {
        struct {
            struct thread *next;
            struct thread *prev;
        } ready_list;
        struct {
            struct thread *next;
            struct thread *prev;
        } block_list;
    };

    //!< Поток также входит в список потоков, приналлежащих процессу
    struct {
        struct thread *next;
        struct thread *prev;
    } inproc_list;

    // TODO Несмотря на реализацию быстрых алгоритмов в ядре с логарифмической сходимостью
    // присутствует также применение списков выше. В некоторых случаях работа по спискам
    // будет тормозить ядро. Подумать что когда куда откуда почему зачем и как.

    //! статистика
    struct {
        uint64_t last_sched_time;
        uint64_t max_sched_time;

        unsigned long run;                  //!< время использования процессора
        unsigned long wait;                 //!< время ожидание в очереди на выполнение
        unsigned long preempted;            //!< время ожидания в вытесненном состоянии
        unsigned long call;                 //!< кол-во системных вызовов из потока
        unsigned long over_slice;           //!< кол-во дополнительных квантов для завершения работы потока
    } stat;
};

static inline void thread_block_list_insert (struct thread *prev, struct thread *thr)
{
    thr->block_list.prev = prev;
    if (prev->block_list.next) {
        thr->block_list.next = prev->block_list.next;
        prev->block_list.next->block_list.prev = thr;
    }
    prev->block_list.next = thr;
}

static inline void thread_block_list_remove (struct thread *thr)
{
    if (!thr->block_list.prev && !thr->block_list.next) {
        return;
    } else if (!thr->block_list.prev) {
        thr->block_list.next->block_list.prev = NULL;
    } else if (!thr->block_list.next) {
        thr->block_list.prev->block_list.next = NULL;
    } else {
        thr->block_list.prev->block_list.next = thr->block_list.next;
        thr->block_list.next->block_list.prev = thr->block_list.prev;
    }
    thr->block_list.next = thr->block_list.prev = NULL;
}

/** \brief Инициализация аллокатора потоков.
     Аллокатор может быть построен на базе массива, списка или кч-дерева,
     его нужно проинициализировать.
 */
void thread_allocator_init();
void thread_allocator_lock();
void thread_allocator_unlock();
void thread_lock(struct thread *thr);
void thread_unlock(struct thread *thr);

/** \brief  Инициализация нового потока, включая первый, без постановки в диспетчер.

    Выполняется аллокатором потоков. Поток всегда принадлежит какому-то процессу.

    По текущей концепции нехватка памяти или свободных номеров tid
        при создании потока фатальна для системы.
        Эта концепция позволяет создавать надежные системы, когда неправильная обработка ситуаций
        невозможности создания новых потоков в прикладном коде или ее отсутствие
        не приведет к неправильно функционирующему изделию
        (должно приводить к аппаратному сбросу по сторожевому таймеру в штатной работе)

    \param p            процесс
    \param entry        стартовая функция исполнения
    \param stack_size   объем стека в байтах (будет приведен к числу страниц)
    \param ts           квант времени в тиках
    \param arg          аргументы потока
    \param OUT thr      указатель на структуру

    \return enum err = OK
 */
int thread_init(struct thread_attr *attrs, struct process *p, struct thread **thr);

/** \brief  Получение от аллокатора потоков ссылки на объект потока по его номеру
    \return NULL если не зарегистрирован
 */
struct thread* get_thr (int tid);

void thread_run(struct thread *thr);

int thread_free(struct thread *thr);


static inline int thread_preemptable(struct thread *current, struct thread *pending) {
    return (pending->prio < current->prio);
}


/**
    \Brief Основная функция смены контекста процессора на целевой поток текущего или другого процесса.

    Обязательное условие: {to} не должен быть пустым, {from} может быть пустым


    Возможны следующие случаи:
    1) Текущий поток исполнялся в режиме пользователя и был прерван прерыванием с вытеснением
    2) Текущий поток выполнял системный вызов в контексте ядра и
        a) был прерван прерыванием с вытеснением
        b) необходимо завершить его выполнение,
        возвращаемое значение должно быть уже записано в возвращаемый контекст режима пользователя
        c) был переведен в другое состояние

    Все случаи, кроме 2).c), не требуют сохранения состояния SVC и возврата в точку вызова функции
    thread_context_switch() и sched_switch(). Для них сеанс SVC должен быть прерван
    и переведен в исходное состояние перед прерыванием или системным вызовом.

    Случай 2).c) требует обратного возврата и продолжения работы кода ядра после диспетчеризации.
    Он является основным при выполнении действий ядра по обработке системных вызовов, которые приводят
    к блокировке потока. Например, при вызове os_syn_wait для захвата блокировки (семафора, мьютекса),
    код ядра может перевести поток в блокировку, при этом необходимо переключиться на какой-либо другой поток и
    вернуться после того, как объект блокировки будет захвачен

    kreturn = 1,- необходимо сохранить текущее состояние сеанса ядра перед переключением и продолжить работу
    в случае обратной передачи управления потоку (возможно только при отсутствии уже сохраненного контекста ядра,
    то есть при отсутствии прерванного сеанса ядра обработчика системного вызова)
 */
extern void call_thread_switch_s(struct thread *from, struct thread *to, int kreturn, void *sp);
void thread_switch(struct thread *from, struct thread *to, struct cpu_context *retctx);


static inline void thread_mutex_block (struct thread *thr, mutex_t *m) {
    thr->state = BLOCKED;
    thr->block.type = SYN_MUTEX;
    thr->block.object.mutex = m;
}

static inline void thread_sem_block (struct thread *thr, semaphore_t *sem) {
    thr->state = BLOCKED;
    thr->block.type = SYN_SEMAPHORE;
    thr->block.object.sem = sem;
}

static inline void thread_barrier_block (struct thread *thr, barrier_t *b) {

    thr->state = BLOCKED;
    thr->block.type = SYN_BARRIER;
    thr->block.object.brr = b;
}

/**
 * Разблокирование потока по внешнему событию, например по удалению объекта синхронизации.
 * Выполняется отмена взведенного таймаута ожидания, если он был установлен,
 * но переменная block_evt не чистится.
 * Значение block_evt == NULL является признаком сработавшего таймаута
 */
static inline void thread_cancel_evt_unblock(struct thread *thr) {
    if(thr->block_evt != NULL) {
        kevent_cancel(thr->block_evt);
    }
    thr->state = READY;
    thr->block.object.ref = NULL;
    thr->block_list.prev = NULL;
    thr->block_list.next = NULL;
}

/**
 * Прерывание ожидания потока по тайм-ауту, вызывается из диспетчера задач
 */
static inline void thread_syn_cancel(struct thread *thr) {
    switch(thr->block.type) {
    case SYN_MUTEX:
        if(mutex_wait_cancel(thr->block.object.mutex, thr) != OK) {
            return;
        }
        break;
    case SYN_SEMAPHORE:
        if(sem_wait_cancel(thr->block.object.sem, thr) != OK) {
            return;
        }
        break;
    case SYN_BARRIER:
        if(barrier_wait_cancel(thr->block.object.brr, thr) != OK) {
            return;
        }
        break;
    default:
        return;
    }
    thr->state = READY;
    thr->block.object.ref = NULL;
    thr->block_list.prev = NULL;
    thr->block_list.next = NULL;
}

static inline void thread_sleep_block (struct thread *thr) {
    thr->state = BLOCKED;
    thr->block.type = SLEEP;
    thr->time_sum = 0;
}

static inline void thread_sleep_unblock(struct thread *thr) {
    thr->state = READY;
    thr->time_sum = 0;
}


static inline void thread_send_block (struct thread *thr, struct connection *connection, size_t size)
{
    thr->state = BLOCKED;
    thr->block.type = SEND;
    thr->block.object.send.connection = connection;
    thr->block.object.send.size = size;
    thr->block_list.next = thr->block_list.prev = NULL;
}

static inline void thread_connect_block (struct thread *thr, struct connection *connection)
{
    thr->state = BLOCKED;
    thr->block.type = CONNECT;
    thr->block.object.connection = connection;
    thr->block_list.next = thr->block_list.prev = NULL;
}

static inline void thread_wait_connect_block (struct thread *thr, struct channel *channel)
{
    thr->state = BLOCKED;
    thr->block.type = WAIT_CONNECT;
    thr->block.object.channel = channel;
    thr->block_list.next = thr->block_list.prev = NULL;
}

static inline void thread_receive_block (struct thread *thr, struct channel *channel)
{
    thr->state = BLOCKED;
    thr->block.type = RECEIVE;
    thr->block.object.channel = channel;
    thr->block_list.next = thr->block_list.prev = NULL;
}

static inline void thread_unblock (struct thread *thr)
{
    if (thr->state != BLOCKED)
        return;
    thread_block_list_remove(thr);
    memset(&thr->block, 0, sizeof(thr->block));
    thr->state = READY;
}

static inline void thread_restart (struct thread *thr)
{
//    thr->regs.pc = (unsigned int)thr->entry;
}

static inline int thread_stop_current(struct thread *thr) {
    thr->state = STOPPED;
    thr->time_sum = 0;
    return OK;
}

/**
 * Функция завершения потока, которая должна вызываться из пользовательского режима
 * автоматически при выходе из функции entry
 */
void thread_exit_user_callback();

/**
 * Основной метод завершения текущего потока из режима ядра
 */
void thread_exit(struct thread *current);

/**
 * Метод окончательной очистки ресурсов, занятых потоком (включая структуру потока).
 * Должен выполняться в безопасном контексте ядра в момент переключения на другой поток.
 */
void thread_finalize (struct thread *thr);

#endif
