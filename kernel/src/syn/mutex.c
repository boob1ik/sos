#include <arch.h>
#include <sched.h>
#include <event.h>
#include <common/utils.h>
#include "mutex.h"

int mutex_init (mutex_t *m, syn_t *s)
{
    if (s == NULL) {
//        m->type = MUTEX_TYPE_PSHARED;
        m->cnt = &m->__cnt;
        m->owner_tid = &m->__owner_tid;
    } else {
        switch (s->type) {
        case MUTEX_TYPE_PSHARED:
            m->cnt = &m->__cnt;
            m->owner_tid = &m->__owner_tid;
            break;
        case MUTEX_TYPE_PLOCAL:
            m->cnt = &s->cnt;
            m->owner_tid = (int *) &s->owner_tid;
            break;
        default:
            return ERR_ILLEGAL_ARGS;
        }
    }
    *m->owner_tid = 0;
    m->cnt->val = 1;
    m->wait_first = NULL;
    m->wait_last = NULL;
    spinlock_init(&m->wait_lock);
    return OK;
}

// С мьютексами работаем только в вытесняемом контексте,
// т.к. это объект длительной синхронизации
// (с разрешенными прерываниями, обычно всегда в режиме SVC, т.к.
// работаем с вытеснением в SVC всегда когда не лочимся на объекте ядра)
// Следствие: нельзя применять внутри синхронизации по объекту ядра
//
int mutex_lock (mutex_t *m, struct thread *thr, uint64_t ns)
{
    int res = OK;
    int cnt = atomic_sub_return(1, m->cnt);
    if (cnt == 0) {
        *m->owner_tid = (thr == NULL) ? 0 : thr->tid;
        // текущий поток является первым и единственным желающим захватить мьютекс
        return res;
    }
    // TODO в вытесняемом коде наверно необходимо сначала залочить очередь а потом атомарно
    // декремент выполнить и на основе значения принять решение, так как
    // в данной точке может произойти вытеснение и мьютекс освободиться,
    // поэтому необходимость в обновлении очереди может отпасть.
    // Если того не делать, то может появится брошенная очередь ожидающих, бесконечно
    // зависших потоков так как тот кто освобождал отработал корректно и освободил очередь
    // полностью.
    // TODO ПРОВЕРИТЬ РАССУЖДЕНИЯ и перенести код выше за лок ниже!

    // Очередь ожидания не пуста и теперь:
    // - блокируем вытеснение, чтобы быстро поправить очередь,
    // - захватываем очередь ожидающих потоков на мьютексе и корректируем ее,
    // - меняем состояние текущего потока,
    // - запускаем диспетчер для переключения на готовый для исполнения поток
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&m->wait_lock);
    thr->block_list.next = NULL;
    if (m->wait_last != NULL) {
        m->wait_last->block_list.next = thr;
        thr->block_list.prev = m->wait_last;
    } else {
        thr->block_list.prev = NULL;
        m->wait_first = thr;
    }
    m->wait_last = thr;
    thread_mutex_block(thr, m);
    if (ns != TIMEOUT_INFINITY) {
        // TODO этот код лучше вынести ниже за unlock(wait_lock),
        // но можно ли / нужно ли ?
        kevent_store_lock();
        thr->block_evt = kevent_insert(ns, thr);
        kevent_store_unlock();
    }
    spinlock_unlock(&m->wait_lock);
    sched_switch(SCHED_SWITCH_SAVE_AND_RET);
    // сюда вернемся только после события по таймауту или при захвате мьютекса текущим потоком
    // (поток не получит управление до передачи ему мьютекса во владение)
    // Результат находится в контексте потока, - определяем является ли владельцем мьютекса
    // текущий поток или нет и возвращаемся к работе

    if (ns != TIMEOUT_INFINITY) {
        if (thr->block_evt == NULL) {
            res = ERR_TIMEOUT;
        } else {
            thr->block_evt = NULL;
            if (*m->owner_tid != thr->tid) {
                res = ERR_BUSY;
            }
        }
    } else {
        if (*m->owner_tid != thr->tid) {
            res = ERR_BUSY;
        }
    }
    interrupt_enable_s(s);
    return res;
}

// Необходимо обеспечить прерывание блокировки заданного потока на мьютексе.
// Это может быть сделано путем его удаления из очереди ожидающих(блокированных)
int mutex_wait_cancel (mutex_t *m, struct thread *thr)
{
    struct thread *next_thr;
    int res = ERR;
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&m->wait_lock);

    if (thr == NULL) {
        // будим все потоки, для случая удаления владельцем объекта синхронизации,
        // чтобы исключить возможные дедлоки в данном случае
        thr = m->wait_first;
        sched_lock();
        while (thr != NULL) {
            next_thr = thr->block_list.next;
            thread_cancel_evt_unblock(thr);
            enqueue(thr);
            atomic_add_return(1, m->cnt);
            thr = next_thr;
        }
        sched_unlock();

        spinlock_unlock(&m->wait_lock);
        interrupt_enable_s(s);
        return OK;
    }

    if ((thr->block.type != SYN_MUTEX) || (thr->block.object.mutex != m)) {
//        panic("mutex_lock_cancel error");
        spinlock_unlock(&m->wait_lock);
        interrupt_enable_s(s);
        return res;
    }
    // поток должен быть в списке, поэтому искать его там не нужно

    if (thr->block_list.prev != NULL) {
        thr->block_list.prev->block_list.next = thr->block_list.next;
    } else {
        // был первым, обновляем
        m->wait_first = thr->block_list.next;
    }
    if (thr->block_list.next != NULL) {
        thr->block_list.next->block_list.prev = thr->block_list.prev;
    } else {
        // был последним, обновляем
        m->wait_last = thr->block_list.next;
    }
    thr->block_list.prev = NULL;
    thr->block_list.next = NULL;
//    atomic_inc_unless_return(m->cnt, 1);
    atomic_add_return(1, m->cnt);
    res = OK;
    spinlock_unlock(&m->wait_lock);
    interrupt_enable_s(s);
    return res;
}

int mutex_trylock (mutex_t *m, struct thread *thr)
{
    if (atomic_dec_if_one(m->cnt) == 0) {
        // текущий поток монопольно захватитил мьютекс без конкурентов
        *m->owner_tid = (thr == NULL) ? 0 : thr->tid;
        return OK;
    }
    return ERR_BUSY;
}

/**
 * TODO
 * Для многоядерной реализации требуется внимание к одновременному исполнению
 * на двух разных ядрах mutex_unlock и разблокировке ожидающего на мьютексе потока,
 * а также срабатыванию события по этому же потоку и выполнение для него mutex_lock_cancel.
 * Требуется корректная отработка по всем вариантам. То же самое для семафоров и барьеров
 */
int mutex_unlock (mutex_t *m, struct thread *thr)
{
    int cnt;
    if (((thr != NULL) && (*m->owner_tid != thr->tid)) || ((thr == NULL) && (*m->owner_tid != 0))) {
        // текущий поток не владел мьютексом, владельцем является другой поток
        return ERR;
    }
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&m->wait_lock);
    thr = m->wait_first;
    if (thr == NULL) {
        // очередь ожидающих пустая, просто освобождаем и дальше работаем
        *m->owner_tid = 0;
        cnt = atomic_add_return(1, m->cnt); //atomic_inc_unless_return(m->cnt, 1);
        if (cnt < 0) {
            // проверка на корректность освобождений нужна ли?
            // и что делать если ошибка??? TODO
            m->cnt->val = 1; // пока отказоустойчивый код делаем
        }
        spinlock_unlock(&m->wait_lock);
        interrupt_enable_s(s);
        return OK;
    }
    // очередь ожидающих не пустая, разблокируем нового владельца мьютекса
    *m->owner_tid = thr->tid;
    m->wait_first = thr->block_list.next;
    if (m->wait_first != NULL) {
        m->wait_first->block_list.prev = NULL;
    } else {
        // чистим последнего, был один в очереди первый и последний
        m->wait_last = NULL;
    }
//    thr->list.next = NULL;
//    atomic_inc_unless_return(m->cnt, 1);
    atomic_add_return(1, m->cnt);
    thread_cancel_evt_unblock(thr); // @TODO возможно здесь можно придумать чтото более правильное
    // очередь ожидающих откорректирована, поскорее освобождаем ее спинлок и включаем вытеснение
    spinlock_unlock(&m->wait_lock);
    interrupt_enable_s(s);

    //thread_run(thr);
    sched_lock();
    enqueue(thr);
    sched_unlock();
//    sched_enqueue_switch(thr, SCHED_SWITCH_SAVE_AND_RET);
    return OK;
}

