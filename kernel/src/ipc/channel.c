#include "channel.h"
#include "mem/vm.h"
#include "mem/kmem.h"
#include "sched.h"
#include "connection.h"
#include "syn/ksyn.h"
#include "common/log.h"
#include "secure.h"
#include "common/utils.h"
#include <os.h>
#include "common/error.h"
#include "syn/ksyn.h"
#include "pathname.h"

struct channel* lock_channel (struct process *proc, int id)
{
    struct res_header *hdr = NULL;
    resm_search_and_lock(proc->channels, id, &hdr);
    if (!hdr)
        return NULL;
    struct channel *channel = resm_get_ref(hdr);
    channel->hdr = hdr;
    return channel;
}


void unlock_channel (struct channel *channel)
{
    struct res_header *hdr = channel->hdr;
    channel->hdr = NULL;
    resm_unlock(channel->owner->channels, hdr);
}


struct channel * lock_channel_pathname (char *pathname, char **suffix)
{
    pathname_t pathname_rootns = lock_pathname(pathname, suffix);
    if (!pathname_rootns)
        return NULL;
    struct channel *channel = get_pathname_ref(pathname_rootns);
    lock_channel(channel->owner, channel->id);
    channel->pathname_use++;
    unlock_pathname(pathname_rootns);
    return channel;
}


void unlock_channel_pathname (struct channel *channel)
{
    if (channel->type != CHANNEL_PRIVATE)
        channel->pathname_use--;
}


static void clr_connecting_list (struct channel *channel)
{
    struct thread *thr = channel->connecting.open.head;
    sched_lock();
    while (thr) {
        thr->block.object.connection->connecting->err = ERR_BUSY;
        struct thread *thr_next = thr->block_list.next;
        thread_unblock(thr);
        enqueue(thr);
        thr = thr_next;
    }
    sched_unlock();
}


//static void kconnect (char *pathname)
//{
//    int conid = 0;
//    if (memcmp(pathname, LOGGER_CHANNEL, strlen(LOGGER_CHANNEL)) == 0) {
//        set_log_target(LOG_TARGET_MSG_CHANNEL);
//    } else if (memcmp(pathname, SECURE_CHANNEL, strlen(SECURE_CHANNEL)) == 0) {
//        secure_init();
//    }
//}


static struct channel * create_channel (void *buf, size_t size, unsigned long flags, channel_type_t type, struct process *proc, pathname_t *pathname)
{
    int id = 0;
    struct res_header *hdr = NULL;
    if ((id = resm_create_and_lock(proc->channels, RES_ID_GENERATE, 0, &hdr)) < 0) {
        return NULL;
    }

    struct channel *channel = kmalloc(sizeof(*channel));
    channel->hdr = hdr;
    resm_set_ref(hdr, (void*)channel);
    channel->buf.data = buf;
    channel->buf.size = size;
    channel->buf.head = channel->buf.tail = channel->buf.data;
    channel->pathname = pathname;
    channel->msgs = 0;
    channel->owner = proc;
    channel->zombie = false;
    channel->type = type;
    channel->flags = flags;
    channel->id = id;
    channel->receive.head = channel->receive.tail = NULL;
    channel->send.head = channel->send.tail = NULL;
    channel->pathname_use = 0;
    channel->connecting.open.head = channel->connecting.open.tail = NULL;
    channel->connecting.wait.head = channel->connecting.wait.tail = NULL;
    rb_tree_init(&channel->connections);
    rb_tree_init(&channel->connecting.complete);

    if (pathname)
        set_pathname_ref(pathname, (void*)channel);
    return channel;
}


static void* create_buf (struct process *proc, size_t size)
{
//    if (proc == &kproc) {
//        //    if (kernel) {
//        //        void *adr = kmalloc_aligned(size, mmu_page_size());
//        //        struct seg *seg = seg_create(kmap, adr, size, attr);
//        //        mmap(thr->proc->mmap, seg);
//        //        return adr;
//        //    }
//        return NULL;
//    }

    const mem_attributes_t attr = {
        .os_access = MEM_ACCESS_RW,
        .process_access = MEM_ACCESS_RO,
        .shared = MEM_SHARED_OFF,
        .type = MEM_TYPE_NORMAL,
        .exec = MEM_EXEC_NEVER,
        .inner_cached = MEM_CACHED_WRITE_BACK,
        .outer_cached = MEM_CACHED_WRITE_BACK,
        .security = MEM_SECURITY_OFF,
        .multu_alloc = MEM_MULTU_ALLOC_OFF,
    };
    size_t pages = size / mmu_page_size();
    if (size % mmu_page_size())
        pages += 1;

    return vm_alloc(proc, pages, attr);
}


static void delete_buf (struct process *proc, void *adr)
{
    vm_free(proc, adr);
}


int open_channel (struct thread *thr, channel_type_t type, char *pathname, size_t size, unsigned long flags)
{
    struct process *caller = thr->proc;

    pathname_t *pathname_ns = NULL;
    if (pathname) {
        pathname_ns = create_pathname(pathname);
        if (!pathname_ns)
            return ERROR(ERR_IPC_PATHNAME);
    }

    void *adr = create_buf(caller, size);
    if (!adr) {
        delete_pathname(pathname_ns);
        return ERROR(ERR_NO_MEM);
    }

    struct channel *channel = create_channel(adr, size, flags, type, caller, pathname_ns);
    if (!channel) {
        delete_buf(caller, adr);
        delete_pathname(pathname_ns);
        return ERROR(ERR_NO_MEM);
    }

    unlock_pathname(pathname_ns);
    unlock_channel(channel);

    log_info("PID:%d (%s) Open channel %d %s", caller->pid, caller->hdr->pathname, channel->id, pathname ? pathname : "noname");
    return channel->id;
}


static void disconnect (struct connection *connection)
{
    lock_connection(connection->owner, connection->id);
    disconnect_low(connection);
    unlock_connection(connection);
}


static void disconnect_all (struct channel *channel)
{
    if (rb_tree_is_empty(&channel->connections))
        return;

    struct rb_node *node = rb_tree_get_min(&channel->connections);
    for (;;) {
        disconnect((struct connection *)rb_node_get_key(node));
        if (rb_tree_is_empty(&channel->connections))
            break;
        node = rb_tree_get_nearlarger(node);
    }
}


//static int close_kchannel (int id)
//{
//    struct channel *ch = (struct channel *)res_get_ref(&kproc.channels, id);
//    if (!ch)
//        return ERR_ILLEGAL_ARGS;
//
//    kobject_lock(&ch->lock);
//    close_connections(ch);
//    kfree(ch->buf.data);
//    res_remove(&channels, id);
//    res_remove(&kproc.channels, id);
//    kobject_unlock(&ch->lock);
//    kfree(ch);
//    lprintf("PID:%d (%s) Close kernel channel %d", cur_proc()->pid, cur_proc()->hdr->name, id);
//    return OK;
//}


void senders_unblock (struct channel *ch)
{
    struct thread *thr = ch->send.head;
    while (thr) {
        struct thread *tmp = thr->block_list.next;
        thread_unblock(thr);
        sched_lock();
        enqueue(thr);
        sched_unlock();
        thr = tmp;
    }
}


static void receivers_unblock (struct channel *ch)
{
    struct thread *thr = ch->receive.head;
    while (thr) {
        struct thread *tmp = thr->block_list.next;
        thread_unblock(thr);
        sched_lock();
        enqueue(thr);
        sched_unlock();
        thr = tmp;
    }
}


static void wait_connecting_unblock (struct channel *ch)
{
    struct thread *thr = ch->connecting.wait.head;
    while (thr) {
        struct thread *tmp = thr->block_list.next;
        thread_unblock(thr);
        sched_lock();
        enqueue(thr);
        sched_unlock();
        thr = tmp;
    }
}


void zombie_channel (struct channel *channel)
{
    if (channel->zombie)
        return;
    channel->zombie = true;
    receivers_unblock(channel);
    wait_connecting_unblock(channel);
}


void delete_channel (struct channel *channel)
{
    zombie_channel(channel);
    resm_remove_locked(channel->owner->channels, channel->hdr);
    delete_buf(channel->owner, channel->buf.data);
    delete_pathname(channel->pathname);
    kfree(channel);
}


int close_channel (struct process *proc, int id)
{
    struct channel *channel = lock_channel(proc, id);
    if (!channel)
        return ERROR(ERR_ILLEGAL_ARGS);

    disconnect_all(channel);
    delete_channel(channel);
    log_info("PID:%d (%s) Close channel %d", proc->pid, proc->hdr->pathname, id);
    return OK;
}


static void _close_channel (res_container_t *c, int id, struct res_header *hdr)
{
    struct channel *channel = resm_get_ref(hdr);
    disconnect_all(channel);
    zombie_channel(channel);
    delete_buf(channel->owner, channel->buf.data);
    delete_pathname(channel->pathname);
    kfree(channel);
}


void close_channels (struct process *proc)
{
    resm_container_free(proc->channels, _close_channel);
}


static void thread_wait_connecting (struct thread *thr, struct channel *channel)
{
    if (!channel->connecting.wait.head) {
        channel->connecting.wait.head = thr;
    } else {
        thread_block_list_insert(channel->connecting.wait.tail, thr);
    }

    channel->connecting.wait.tail = thr;
    thread_wait_connect_block(thr, channel);
}


static inline bool no_connecting (struct channel *channel)
{
    return channel->connecting.open.head == NULL ? true : false;
}


int channel_connection_wait (struct thread *thr, int chid, struct connection_info *info, uint64_t timeout)
{
    struct channel *channel = lock_channel(thr->proc, chid);
    if (!channel)
        return ERROR(ERR_ILLEGAL_ARGS);

    while (no_connecting(channel)) {
        thread_wait_connecting(thr, channel);
        unlock_channel(channel);
        sched_switch(SCHED_SWITCH_SAVE_AND_RET);
        lock_channel(thr->proc, chid);
    }

    struct connection *connection = channel->connecting.open.head->block.object.connection;
    lock_connection(connection->owner, connection->id);

    struct rb_node *node = kmalloc(sizeof(*node));
    rb_node_init(node);
    rb_node_set_key(node, (size_t)thr);
    rb_node_set_data(node, (void*)connection);
    rb_tree_insert(&channel->connecting.complete, node);

    info->pid = connection->connecting->info->pid;
    info->conid = connection->connecting->info->conid;
    memcpy(info->pathname.absolute, connection->connecting->info->pathname.absolute, PATHNAME_MAX_BUF);
    info->pathname.suffix = info->pathname.absolute + (connection->connecting->info->pathname.suffix - connection->connecting->info->pathname.absolute);

    channel->connecting.open.head = channel->connecting.open.head->block_list.next;
    channel->connecting.wait.head = thr->block_list.next;

    unlock_connection(connection);
    unlock_channel(channel);
    return OK;
}


static struct connection * pick_connection (struct channel *channel, struct thread *thr)
{
    struct rb_node *node = rb_tree_search(&channel->connecting.complete, (size_t)thr);
    if (!node) {
        return NULL;
    }
    struct connection *connection = rb_node_get_data(node);
    rb_tree_remove(&channel->connecting.complete, node);
    kfree(node);
    return connection;
}


static void unblock_connecting_thread (struct connection *connection)
{
    thread_unblock(connection->connecting->thr);
    sched_lock();
    enqueue(connection->connecting->thr);
    sched_unlock();
}


int channel_connection_complete (struct thread *thr, int chid, enum connection_cmd cmd, int result)
{
    struct channel *channel = lock_channel(thr->proc, chid);
    if (!channel)
        return ERROR(ERR_ILLEGAL_ARGS);

    struct connection *connection = pick_connection(channel, thr);
    if (!connection) {
        unlock_channel(channel);
        return ERROR(ERR_IPC_NA);
    }
    lock_connection(connection->owner, connection->id);

    if (connection->connecting->channel != channel) {
        unlock_connection(connection);
        unlock_channel(channel);
        return ERROR(ERR_IPC_ILLEGAL_CHANNEL);
    }

    int res = ERR;
    switch (cmd) {
    case CONNECTION_TO_PRIVATE:
        unlock_channel_pathname(channel);
        unlock_channel(channel);
        channel = lock_channel(thr->proc, result);
        if (!channel || channel->type != CHANNEL_PRIVATE) {
            res = ERR_IPC_ILLEGAL_PRIVATE;
            break;
        }
        // THROUGH
    case CONNECTION_ALLOW:
        res = reply_connect(thr->proc, connection);
        if (res != OK)
            break;

        connect(channel, connection);
        if (channel->flags | CHANNEL_SINGLE_CONNECTION)
            clr_connecting_list(channel);
        break;
    case CONNECTION_DENY:
        res = result;
        break;
    case CONNECTION_CONTINUE:
        unlock_channel_pathname(channel);
        unlock_channel(channel);
        res = redirect_connecting(thr->proc, connection);
        unlock_connection(connection);
        return  res != OK ? ERROR(res) : OK;
    default:
        res = ERR_IPC_ILLEGAL_COMMAND;
    }
    connection->connecting->err = res;
    unlock_channel_pathname(channel);
    unlock_channel(channel);
    unlock_connection(connection);
    unblock_connecting_thread(connection);
    return res != OK ? ERROR(res) : connection->reply ? connection->reply->id : OK;
}
