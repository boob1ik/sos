#include <arch.h>
#include <sched.h>
#include <event.h>
#include <common/utils.h>
#include "barrier.h"

int barrier_init(barrier_t *b, syn_t *s) {
    if(s == NULL) {
        return ERR_ILLEGAL_ARGS;
//        m->type = BARRIER_TYPE_PSHARED;
//        b->cnt = &b->__cnt;
//        b->limit = 2; // ?
    } else {
        switch(s->type) {
        case BARRIER_TYPE_PLOCAL:
        case BARRIER_TYPE_PSHARED:
            b->cnt = &b->__cnt;
            break;
        default:
            return ERR_ILLEGAL_ARGS;
        }
        if(s->limit < 2) {
            return ERR_ILLEGAL_ARGS;
        }
        b->limit = s->limit;
    }
    b->cnt->val = b->limit;
    b->wait_first = NULL;
    b->wait_last = NULL;
    spinlock_init(&b->wait_lock);
    return OK;
}

int barrier_wait(barrier_t *b, struct thread *thr, uint64_t ns) {
    int res = OK;
    int cnt;
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&b->wait_lock);
    cnt = atomic_sub_return(1, b->cnt);
    if(cnt == 0) {
        // разблокируем все ожидающие потоки
        // аккуратнее, так как указатели thr->list применяются как для списка блокированных,
        // так и для списка готовых READY
        sched_lock();
        struct thread *thr = b->wait_first, *next;
        while(thr != NULL) {
            next = thr->block_list.next;
            thread_cancel_evt_unblock(thr);
            enqueue(thr);
            thr = next;
        }
        sched_unlock();
        b->wait_first = NULL;
        b->wait_last = NULL;
        b->cnt->val = b->limit;
        spinlock_unlock(&b->wait_lock);
        interrupt_enable_s(s);
        return res;
    }
    thr->block_list.next = NULL;
    if (b->wait_last != NULL) {
        b->wait_last->block_list.next = thr;
        thr->block_list.prev = b->wait_last;
    } else {
        thr->block_list.prev = NULL;
        b->wait_first = thr;
    }
    b->wait_last = thr;
    thread_barrier_block(thr, b);
    if(ns != TIMEOUT_INFINITY) {
        // TODO этот код лучше вынести ниже за unlock(wait_lock),
        // но можно ли / нужно ли ?
        kevent_store_lock();
        thr->block_evt = kevent_insert(ns, thr);
        kevent_store_unlock();
    }
    spinlock_unlock(&b->wait_lock);
    sched_switch(SCHED_SWITCH_SAVE_AND_RET);
    if(ns != TIMEOUT_INFINITY) {
        if(thr->block_evt == NULL) {
            res = ERR_TIMEOUT;
        } else {
            thr->block_evt = NULL;
        }
    }
    // сюда вернемся только после события по таймауту или
    // при выборке потока из очереди блокированных
    interrupt_enable_s(s);
    return res;
}

int barrier_wait_cancel(barrier_t *b, struct thread *thr) {
    int res = ERR;
    struct thread *next_thr;
    uint32_t s = interrupt_disable_s();
    spinlock_lock(&b->wait_lock);

    if(thr == NULL) {
        // будим все потоки, для случая удаления владельцем объекта синхронизации,
        // чтобы исключить возможные дедлоки в данном случае
        thr = b->wait_first;
        sched_lock();
        while(thr != NULL) {
            next_thr = thr->block_list.next;
            thread_cancel_evt_unblock(thr);
            enqueue(thr);
            atomic_inc_unless_return(b->cnt, b->limit);
            thr = next_thr;
        }
        sched_unlock();
        spinlock_unlock(&b->wait_lock);
        interrupt_enable_s(s);
        return OK;
    }

    if( (thr->block.type != SYN_BARRIER) || (thr->block.object.brr != b) ) {
//        panic("barrier_lock_cancel error");
        spinlock_unlock(&b->wait_lock);
        interrupt_enable_s(s);
        return res;
    }
    // поток должен быть в списке, поэтому искать его там не нужно

    if(thr->block_list.prev != NULL) {
        thr->block_list.prev->block_list.next = thr->block_list.next;
    } else {
        // был первым, обновляем
        b->wait_first = thr->block_list.next;
    }
    if(thr->block_list.next != NULL) {
        thr->block_list.next->block_list.prev = thr->block_list.prev;
    } else {
        // был последним, обновляем
        b->wait_last = thr->block_list.next;
    }
    thr->block_list.prev = NULL;
    thr->block_list.next = NULL;
    atomic_inc_unless_return(b->cnt, b->limit);
    res = OK;
    spinlock_unlock(&b->wait_lock);
    interrupt_enable_s(s);
    return res;
}
