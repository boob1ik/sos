#include <arch.h>
#include <sched.h>
#include <event.h>
#include <common/utils.h>
#include "sem.h"

int sem_init(semaphore_t *sem, syn_t *s) {
    if(s == NULL) {
//        m->type = SEMAPHORE_TYPE_PSHARED;
        sem->cnt = &sem->__cnt;
        sem->limit = 1;
    } else {
        switch(s->type) {
        case SEMAPHORE_TYPE_PSHARED:
            sem->cnt = &sem->__cnt;
            break;
        case SEMAPHORE_TYPE_PLOCAL:
            sem->cnt = &s->cnt;
            break;
        default:
            return ERR_ILLEGAL_ARGS;
        }
        if(s->limit < 1) {
            return ERR_ILLEGAL_ARGS;
        }
        sem->limit = s->limit;
    }
    sem->cnt->val = sem->limit;
    sem->wait_first = NULL;
    sem->wait_last = NULL;
    spinlock_init(&sem->wait_lock);
    return OK;
}

int sem_lock(semaphore_t *sem, struct thread *thr, uint64_t ns) {
    int res = OK;
    int cnt;
    // Ставим в очередь ожидания:
    // - блокируем вытеснение, чтобы быстро поправить очередь,
    // - захватываем очередь ожидающих потоков и корректируем ее,
    // - меняем состояние текущего потока,
    // - запускаем диспетчер для переключения на готовый для исполнения поток
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&sem->wait_lock);
    cnt = atomic_sub_return(1, sem->cnt);
    if(cnt >= 0) {
        spinlock_unlock(&sem->wait_lock);
        interrupt_enable_s(s);
        return res;
    }
    thr->block_list.next = NULL;
    if (sem->wait_last != NULL) {
        sem->wait_last->block_list.next = thr;
        thr->block_list.prev = sem->wait_last;
    } else {
        thr->block_list.prev = NULL;
        sem->wait_first = thr;
    }
    sem->wait_last = thr;
    thread_sem_block(thr, sem);
    if(ns != TIMEOUT_INFINITY) {
        // TODO этот код лучше вынести ниже за unlock(wait_lock),
        // но можно ли / нужно ли ?
        kevent_store_lock();
        thr->block_evt = kevent_insert(ns, thr);
        kevent_store_unlock();
    }
    spinlock_unlock(&sem->wait_lock);
    sched_switch(SCHED_SWITCH_SAVE_AND_RET);
    if(ns != TIMEOUT_INFINITY) {
        if(thr->block_evt == NULL) {
            res = ERR_TIMEOUT;
        }
    }
    thr->block_evt = NULL;
    // сюда вернемся только после события по таймауту или
    // при выборке потока из очереди блокированных
    interrupt_enable_s(s);
    return res;
}

int sem_wait_cancel(semaphore_t *sem, struct thread *thr) {
    struct thread *next_thr;
    int res = ERR;
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&sem->wait_lock);

    if(thr == NULL) {
        // будим все потоки, для случая удаления владельцем объекта синхронизации,
        // чтобы исключить возможные дедлоки в данном случае
        thr = sem->wait_first;
        sched_lock();
        while(thr != NULL) {
            next_thr = thr->block_list.next;
            thread_cancel_evt_unblock(thr);
            enqueue(thr);
            atomic_inc_unless_return(sem->cnt, sem->limit);
            thr = next_thr;
        }
        sched_unlock();

        spinlock_unlock(&sem->wait_lock);
        interrupt_enable_s(s);
        return OK;
    }

    if( (thr->block.type != SYN_SEMAPHORE) || (thr->block.object.sem != sem) ) {
//        panic("sem_lock_cancel error");
        spinlock_unlock(&sem->wait_lock);
        interrupt_enable_s(s);
        return res;
    }
    // поток должен быть в списке, поэтому искать его там не нужно

    if(thr->block_list.prev != NULL) {
        thr->block_list.prev->block_list.next = thr->block_list.next;
    } else {
        // был первым, обновляем
        sem->wait_first = thr->block_list.next;
    }
    if(thr->block_list.next != NULL) {
        thr->block_list.next->block_list.prev = thr->block_list.prev;
    } else {
        // был последним, обновляем
        sem->wait_last = thr->block_list.next;
    }
    thr->block_list.prev = NULL;
    thr->block_list.next = NULL;
    atomic_inc_unless_return(sem->cnt, sem->limit);
    res = OK;
    spinlock_unlock(&sem->wait_lock);
    interrupt_enable_s(s);
    return res;
}

int sem_trylock(semaphore_t *sem, struct thread *thr) {
    if (atomic_dec_if_gz(sem->cnt) == 0) {
        return OK;
    }
    return ERR;
}

int sem_unlock(semaphore_t *sem, struct thread *thr) {
    int cnt;
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&sem->wait_lock);
    thr = sem->wait_first;
    if(thr == NULL) {
        // очередь ожидающих пустая, просто освобождаем и дальше работаем
        cnt = atomic_inc_unless_return(sem->cnt, sem->limit);
        if(cnt < 0) {
            // проверка на корректность освобождений нужна ли?
            // и что делать если ошибка??? TODO
            // контекст не вытесняемый, залочили спинлок, значит вроде безопасно анализировать
            // значение счетчика и принимать решение
            sem->cnt->val = 1; // пока отказоустойчивый код делаем
        }
        spinlock_unlock(&sem->wait_lock);
        interrupt_enable_s(s);
        return OK;
    }
    // очередь ожидающих не пустая, разблокируем следующий поток
    sem->wait_first = thr->block_list.next;
    if(sem->wait_first != NULL) {
        sem->wait_first->block_list.prev = NULL;
    } else {
        // чистим последнего, был один в очереди первый и последний
        sem->wait_last = NULL;
    }
    thr->block_list.next = NULL;
    atomic_inc_unless_return(sem->cnt, sem->limit);
    thread_cancel_evt_unblock(thr); // @TODO возможно здесь можно придумать чтото более правильное
    // очередь ожидающих откорректирована, поскорее освобождаем ее спинлок и включаем вытеснение
    spinlock_unlock(&sem->wait_lock);
    interrupt_enable_s(s);

    //thread_run(thr);
    sched_lock();
    enqueue(thr);
    sched_unlock();
//    sched_enqueue_switch(thr, SCHED_SWITCH_SAVE_AND_RET);
    return OK;
}
