#include "msg.h"
#include "connection.h"
#include "channel.h"
#include "mem/vm.h"
#include "thread.h"
#include "proc.h"
#include "sched.h"
#include <common/utils.h>
#include <os.h>
#include "common\error.h"

struct emsg {
    size_t size;
    bool drop;
    unsigned long flags;
    struct msg m;
};

static inline sys_msg_type_t get_msg_type (struct emsg *em)
{
    return *(sys_msg_type_t*)em->m.sys.ptr;
}

static inline void* get_msg_sys (struct emsg *em)
{
    return em->m.sys.ptr;
}

static size_t sys_size (sys_msg_type_t type)
{
    if (!type)
        return 0;
    switch (type) {
    case SYS_MSG_MEM_SEND:
    case SYS_MSG_MEM_SHARE:
        return sizeof(struct sys_msg_mem);
    case SYS_MSG_SIGNAL:
        return sizeof(struct sys_msg_signal_event);
#ifdef DEBUG
    default:
        asm volatile ("bkpt");
#endif
    }
    return 0;
}

static bool drop_msg (struct channel * const ch)
{
    if (!ch->msgs)
        return false;

    struct emsg *em = (struct emsg *)ch->buf.head;
    if (em->drop) {
        ch->buf.head += em->size;
        ch->msgs--;
        return true;
    }
    return false;
}

static struct emsg* pop_msg (struct channel * const ch)
{
    if (!ch->msgs)
        return NULL;

    while (drop_msg(ch)) {
        if (ch->buf.head >= ch->buf.data + ch->buf.size)
            ch->buf.head = ch->buf.data;
    }

    if (!ch->msgs) {
        ch->buf.head = ch->buf.tail = ch->buf.data;
        return NULL;
    }

    struct emsg *em = (struct emsg *)ch->buf.head;
    em->drop = true;
    return em;
}

static size_t copy_msg (uint8_t *buf, struct emsg * const em)
{
    struct emsg *em_buf = (struct emsg *)buf;
    *em_buf = *em;
    buf += sizeof(*em);

    if (get_msg_type(em)) {
        size_t size = sys_size(get_msg_type(em));
        memcpy(buf, get_msg_sys(em), size);
        buf += size;
    }

    memcpy(buf, em->m.data, em->m.size);
    em_buf->m.data = buf;

    return em_buf->size;
}

static void push_dummy_msg (uint8_t * const tail, const size_t size)
{
    struct emsg *emsg = (struct emsg *)tail;
    emsg->drop = true;
    emsg->size = size;
}

static size_t align_tail (uint8_t * const tail)
{
    size_t stub_size = 0;
    if ((uint32_t)tail % 8) {
        uint8_t *align_tail = (uint8_t*) ALIGN(tail, 8);
        stub_size = (size_t)(align_tail - tail);
        memset(tail, 0, stub_size);
    }
    return stub_size;
}

static inline bool empty_channel (const struct channel * const ch)
{
    return !ch->msgs;
}

static inline bool convolution (const struct channel * const ch)
{
    return ch->buf.head < ch->buf.tail;
}

static size_t get_free_size (const struct channel * const ch)
{
    if (!ch->msgs)
        return ch->buf.size;

    if (convolution(ch)) {
        size_t tail_free = ch->buf.data + ch->buf.size - ch->buf.tail;
        size_t head_free = ch->buf.head - ch->buf.data;
        return tail_free + head_free;
    }

    return ch->buf.tail - ch->buf.head;
}

/* буфер со сверткой нужно учитывать пустоты до сверки, если туда не влезает сообщение */
static bool push_msg (struct channel * const ch, struct emsg * const emsg)
{
    if (emsg->size > ch->buf.size) {
        // такое сообщение никогда не влезет
        return false;
    }

    size_t need_size = emsg->size;
    if (need_size > get_free_size(ch))
        return false;

    if (!empty_channel(ch)) {
        size_t tail_size = (ch->buf.data + ch->buf.size) - ch->buf.tail;
        size_t head_size = ch->buf.head - ch->buf.data;
        if (tail_size < need_size + sizeof(struct emsg)) {
            if (head_size < need_size) {
                return false;
            } else {
                push_dummy_msg(ch->buf.tail, tail_size);
                ch->msgs++;
                ch->buf.tail = ch->buf.data;
            }
        }
    }

    ch->buf.tail += copy_msg(ch->buf.tail, emsg);
    ch->msgs++;

    ch->buf.tail += align_tail(ch->buf.tail);

    if (ch->buf.tail == ch->buf.data + ch->buf.size)
        ch->buf.tail = ch->buf.data;

    return true;
}

static void block_sender (struct thread * const sender, struct connection * const connection, const size_t size)
{
    thread_send_block(sender, connection, size);

    if (!connection->channel->send.head) {
        connection->channel->send.head = sender;
    } else
        thread_block_list_insert(connection->channel->send.tail, sender);
    connection->channel->send.tail = sender;
}

static void try_unblock_senders (struct channel * const ch)
{
    sched_lock();
    for (;;) {
        struct thread *sender = ch->send.head;

        if (!sender)
            break;

        if (ch->send.head->block.object.send.size > get_free_size(ch))
            break;

        ch->send.head = sender->block_list.next;
        thread_unblock(sender);
        enqueue(sender);
    }
    sched_unlock();
}

static void block_receiver (struct thread * const receiver, struct channel * const channel)
{
    thread_receive_block(receiver, channel);

    if (!channel->receive.head) {
        channel->receive.head = receiver;
    } else
        thread_block_list_insert(channel->receive.tail, receiver);
    channel->receive.tail = receiver;
}

static void try_unblock_receiver (struct channel * const channel)
{
    struct thread *receiver = channel->receive.head;
    if (receiver) {
        channel->receive.head = receiver->block_list.next;
        thread_unblock(receiver);
        sched_lock();
        enqueue(receiver);
        sched_unlock();
    }
}

static int set_timeout (struct thread * const thr, const uint64_t timeout)
{
    if (timeout < TIMEOUT_MIN)
        return ERROR(ERR_TIMEOUT);

    if (timeout == TIMEOUT_INFINITY)
        return OK;

    kevent_store_lock();
    thr->block_evt = kevent_insert(systime() + timeout, thr);
    kevent_store_unlock();
    return OK;
}


int send (struct thread * const sender, const int conid, struct msg * const msg, const uint64_t timeout, const unsigned long flags)
{
    struct connection *connection = lock_connection(sender->proc, conid);
    if (!connection)
        return ERROR(ERR_ILLEGAL_ARGS);

    if (!connection->channel) {
        unlock_connection(connection);
        return ERROR(ERR_DEAD);
    }
    unlock_connection(connection);

    struct emsg emsg;
    emsg.drop = false;
    emsg.m = *msg;
    emsg.flags = flags;
    emsg.size = sizeof(emsg) + msg->size + sys_size(get_msg_type(&emsg));

    if (get_msg_sys(&emsg)) {
        switch (get_msg_type(&emsg)) {
        case SYS_MSG_MEM_SEND:
        {
            struct sys_msg_mem *mem = (struct sys_msg_mem *)(msg->sys.memcmd);
            int res = vm_seg_move(sender->proc->mmap, sender->proc->mmap, vm_seg_get(sender->proc->mmap, mem->seg.adr));
            if (res != OK) {
                struct channel *channel = lock_channel(connection->channel->owner, connection->channel->id);
                unlock_channel(channel);
                unlock_connection(connection);
                return ERROR(res);
            }
        }
            break;
        case SYS_MSG_MEM_SHARE:
        {
            struct sys_msg_mem *mem = (struct sys_msg_mem *)(msg->sys.memcmd);
            int res = vm_seg_share(sender->proc->mmap, vm_seg_get(sender->proc->mmap, mem->seg.adr));
            if (res != OK) {
                struct channel *channel = lock_channel(connection->channel->owner, connection->channel->id);
                unlock_channel(channel);
                unlock_connection(connection);
                return ERROR(res);
            }
        }
            break;
        case SYS_MSG_SIGNAL:
        {
            break;
        }
        default:
            unlock_connection(connection);
            return ERROR(ERR_ILLEGAL_ARGS);
        }
    }

    for (;;) {
        connection = lock_connection(sender->proc, conid);
        if (!connection || !connection->channel) {
            unlock_connection(connection);
            return ERROR(ERR_DEAD);
        }

        struct channel *channel = lock_channel(connection->channel->owner, connection->channel->id);
        if (push_msg(channel, &emsg)) {
            unlock_channel(channel);
            break;
        }
        unlock_channel(channel);

        int res = set_timeout(sender, timeout);
        if (res != OK) {
            unlock_connection(connection);
            return ERROR(res);
        }

        block_sender(sender, connection, emsg.size);
        unlock_connection(connection);
        sched_switch(SCHED_SWITCH_SAVE_AND_RET);

        if (!sender->block_evt && timeout != TIMEOUT_INFINITY) {
            unlock_connection(connection);
            return ERROR(ERR_TIMEOUT);
        }
        kevent_cancel(sender->block_evt);
    }



    struct channel *channel = lock_channel(connection->channel->owner, connection->channel->id);
    try_unblock_receiver(channel);
    unlock_channel(channel);

    if (emsg.flags & MSG_WAIT_COMPLETE) {
        int res = set_timeout(sender, timeout);
        if (res != OK) {
            unlock_connection(connection);
            return ERROR(res);
        }

        block_sender(sender, connection, emsg.size);
        sched_switch(SCHED_SWITCH_SAVE_AND_RET);

        if (!sender->block_evt && timeout != TIMEOUT_INFINITY) {
            unlock_connection(connection);
            return ERROR(ERR_TIMEOUT);
        }
        kevent_cancel(sender->block_evt);
    }

    unlock_connection(connection);
    return OK;
}

int receive (struct thread * const receiver, const int chid, struct msg ** const msg, const uint64_t timeout)
{
    *msg = NULL;
    struct emsg *emsg = NULL;

    struct channel *channel = lock_channel(receiver->proc, chid);
    if (!channel) {
        return ERROR(ERR_ILLEGAL_ARGS);
    }

    for (;;) {
        try_unblock_senders(channel);

        emsg = pop_msg(channel);
        if (emsg)
            break;

        if (channel->zombie) {
            unlock_channel(channel);
            return ERROR(ERR_DEAD);
        }

        int res = set_timeout(receiver, timeout);
        if (res != OK)
            return ERROR(res);

        block_receiver(receiver, channel);
        unlock_channel(channel);
        sched_switch(SCHED_SWITCH_SAVE_AND_RET);

        if (!receiver->block_evt && timeout != TIMEOUT_INFINITY) {
            return ERROR(ERR_TIMEOUT);
        }
        kevent_cancel(receiver->block_evt);

        channel = lock_channel(receiver->proc, chid);
        if (!channel) {
            return ERROR(ERR_DEAD);
        }
    }
    *msg = &emsg->m;

    if (!(emsg->flags & MSG_WAIT_COMPLETE))
        try_unblock_senders(channel);

    unlock_channel(channel);
    return OK;
}
