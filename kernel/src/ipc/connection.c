#include "connection.h"
#include "stddef.h"
#include "stdbool.h"
#include <os_types.h>
#include <rbtree.h>
#include "proc.h"
#include "sched.h"
#include "mem/kmem.h"
#include "ipc/msg.h"
#include "channel.h"
#include "common/log.h"
#include <os.h>
#include "common/error.h"
#include "syn/ksyn.h"
#include "pathname.h"


static bool permission (struct process *caller, char *pathname)
{
    if (!caller || !pathname)
        return false;

    // TODO
    /**
     \warning
     Тут должно быть обращение к менеджеру безопасности для проверки прав.
     Это основа безопасности межпроцессного обмена.
     */

    /* пока всем все разрешено */

    return true;
}


struct connection* lock_connection (struct process *proc, int id)
{
    struct res_header *hdr = NULL;
    resm_search_and_lock(proc->connections, id, &hdr);
    return resm_get_ref(hdr);
}


void unlock_connection (struct connection *connection)
{
    resm_unlock(connection->owner->connections, connection->hdr);
}


/* Установка соединения с каналом. Выполняется на каждом перенаправлении. */
static void connecting (struct connection *connection, struct channel *channel)
{
    connection->connecting->channel = channel;
    struct thread *thr = connection->connecting->thr;

    if (channel->connecting.open.head) {
        thread_block_list_insert(channel->connecting.open.tail, thr);
    } else {
        channel->connecting.open.head = thr;
    }
    channel->connecting.open.tail = thr;


    if (channel->connecting.wait.head) {
        struct thread *waiter = channel->connecting.wait.head;
        channel->connecting.wait.head = waiter->block_list.next;
        thread_unblock(waiter);

        unlock_connection(connection);
        unlock_channel(channel);
        sched_lock();
        enqueue(waiter);
        sched_unlock();
        lock_channel(channel->owner, channel->id);
        lock_connection(connection->owner, connection->id);
    }

    unlock_channel(channel);

    if (!connection->channel) {
        thread_connect_block(thr, connection);
        unlock_connection(connection);
        sched_switch(SCHED_SWITCH_SAVE_AND_RET);
        lock_connection(connection->owner, connection->id);
    }
}


static struct connection* create_connection (struct process *owner, int *id)
{
    struct res_header *hdr = NULL;
    if ((*id = resm_create_and_lock(owner->connections, RES_ID_GENERATE, 0, &hdr)) < 0) {
        return NULL;
    }

    struct connection *connection = kmalloc(sizeof(*connection));
    bzero(connection, sizeof(*connection));
    connection->owner = owner;
    connection->channel = NULL;
    connection->connecting = NULL;
    connection->id = *id;
    connection->hdr = hdr;
    resm_set_ref(hdr, (void*)connection);
    return connection;
}


static void delete_connection (struct connection *connection)
{
    resm_remove_locked(connection->owner->connections, connection->hdr);
    kfree(connection);
}


void connect (struct channel *channel, struct connection *connection)
{
    struct rb_node *node = kmalloc(sizeof(*node));
    rb_node_init(node);
    rb_node_set_key(node, (size_t)connection);
    rb_tree_insert(&channel->connections, node);
    connection->channel = channel;
}


void disconnect_low (struct connection *connection)
{
    struct channel *channel = connection->channel;
    connection->channel = NULL;

    struct rb_node *node = rb_tree_search(&channel->connections, (size_t)connection);
    rb_tree_remove(&channel->connections, node);
    kfree(node);

    senders_unblock(channel);
    vm_seg_unshare(connection->owner->mmap, vm_seg_get(channel->owner->mmap, channel->buf.data));
}


static void disconnect (struct connection *connection)
{
    if (!connection->channel)
        return;

    struct channel *channel = connection->channel;
    if (lock_channel(channel->owner, channel->id) != channel)
        return;

    disconnect_low(connection);

    if ((channel->flags & CHANNEL_AUTO_KILL) && !channel->connections.nodes && !channel->pathname_use) {
        zombie_channel(channel);
    }

    unlock_channel(channel);
}


//int open_kconnection (int kchid, int chid_reply)
//{
//    struct channel *kchannel = get_channel(kchid);
//    if (!kchannel)
//        return ERR_ACCESS_DENIED;
//
//    int kconid = kchid;
//    struct connection *connection = create_connection(&kproc, chid_reply, &kconid);
//    mem_attributes_t attr = { HEAP_ATTR };
//    attr.process_access = MEM_ACCESS_RO;
//    struct seg *seg = seg_create(kproc.mmap, kchannel->buf.data, kchannel->buf.size, attr);
//    vm_seg_share(cur_proc()->mmap, seg);
//    connecting(kchannel, connection);
//    return kchid;
//}


int reply_connect (struct process *proc, struct connection *connection)
{
    if (!connection->connecting->reply)
        return OK;

    struct channel *reply_channel = lock_channel(connection->owner, connection->connecting->reply);
    if (!reply_channel) {
        return ERROR(ERR_IPC_REPLY_CHANNEL);
    }

    if (reply_channel->type != CHANNEL_PRIVATE) {
        unlock_channel(reply_channel);
        return ERROR(ERR_IPC_REPLY_CHANNEL);
    }

    int conid = DYNAMIC_ID;
    struct connection *reply_connection = create_connection(proc, &conid);
    if (!reply_connection) {
        unlock_channel(reply_channel);
        return ERROR(ERR_IPC_REPLY_CONNECTION);
    }

    connect(reply_channel, reply_connection);

    if (vm_seg_share(reply_connection->owner->mmap, vm_seg_get(reply_connection->channel->owner->mmap, reply_connection->channel->buf.data)) != OK) {
        delete_connection(reply_connection);
        unlock_channel(reply_channel);
        return ERROR(ERR_IPC_SHARE);
    }

    log_info("PID:%d (%s) Open reply connection %d", proc->pid, proc->hdr->pathname, conid);
    connection->reply = reply_connection;
    unlock_channel(reply_channel);
    unlock_connection(reply_connection);
    return OK;
}


static struct connecting * init_connecting (struct thread *thr, uint64_t timeout, int reply_chid)
{
    struct connecting *connecting = kmalloc(sizeof(*connecting));
    connecting->err = ERR;
    connecting->thr = thr;
    connecting->timeout = timeout;
    connecting->reply = reply_chid;
    connecting->channel = NULL;
    return connecting;
}


static struct connection_info * init_connecting_info (char *pathname, char *suffix, int pid, int conid)
{
    struct connection_info *info = kmalloc(sizeof(*info));
    memcpy(info->pathname.absolute, pathname, strlen(pathname) + 1);
    info->pathname.suffix = suffix ? info->pathname.absolute + (suffix - pathname) : NULL;
    info->pid = pid;
    info->conid = conid;
    return info;
}


int open_connection (struct thread *thr, char *pathname, uint64_t timeout, int reply_chid)
{
    if (!pathname) {
        return ERROR(ERR_IPC_PATHNAME);
    }

    struct process *caller = thr->proc;
    if (!permission(caller, pathname)) {
        return ERROR(ERR_ACCESS_DENIED);
    }

    char *suffix = pathname;
    struct channel *channel = lock_channel_pathname(pathname, &suffix);
    if (!channel)
        return ERROR(ERR_IPC_CHANNEL_NOTFOUND);

    if (channel->flags | CHANNEL_SINGLE_CONNECTION) {
        if (channel->connections.nodes) {
            unlock_channel(channel);
            return ERROR(ERR_BUSY);
        }
    }

    int conid = DYNAMIC_ID;
    struct connection *connection = create_connection(caller, &conid);
    if (!connection) {
        unlock_channel(channel);
        kfree(connection);
        return ERROR(ERR);
    }

    if (channel->flags & CHANNEL_AUTO_CONNECT) {
        connect(channel, connection);
        unlock_channel(channel);
    } else {
        // TODO рефакторинг
        // фоново вызывается unlock_channel(channel);

        connection->connecting = init_connecting(thr, timeout, reply_chid);
        connection->connecting->info = init_connecting_info(pathname, suffix, caller->pid, conid);

        connecting(connection, channel);

        kfree(connection->connecting->info);
        connection->connecting->info = NULL;

        int err = connection->connecting->err;
        if (err != OK) {
            kfree(connection->connecting);
            delete_connection(connection);
            return ERROR(err);
        }

        kfree(connection->connecting);
        connection->connecting = NULL;
    }

    if (vm_seg_share(caller->mmap, vm_seg_get(connection->channel->owner->mmap, connection->channel->buf.data)) != OK) {
        delete_connection(connection);
        return ERROR(ERR);
    }

    log_info("PID:%d (%s) Open connection %d", caller->pid, caller->hdr->pathname, conid);

    unlock_connection(connection);
    return conid;
}


int redirect_connecting (struct process *proc, struct connection *connection)
{
    char *pathname = connection->connecting->info->pathname.absolute;
    char **suffix = &connection->connecting->info->pathname.suffix;
    struct channel *channel = lock_channel_pathname(pathname, suffix);
    if (!channel)
        return ERROR(ERR_IPC_PATHNAME);

    if (channel->type != CHANNEL_PROTECTED || channel->owner->parent != proc)
        return ERROR(ERR_IPC_REDIRECT);

    connecting(connection, channel);
    return OK;
}


int close_connection (struct process *proc, int id)
{
    struct connection *connection = lock_connection(proc, id);
    if (!connection)
        return ERROR(ERR_ILLEGAL_ARGS);

    resm_remove_locked(connection->owner->connections, connection->hdr);
    disconnect(connection);
    if (connection->reply) {
        lock_connection(connection->reply->owner, connection->reply->id);
        disconnect(connection->reply);
        unlock_connection(connection->reply);
    }
    log_info("PID:%d (%s) Close connection %d", proc->pid, proc->hdr->pathname, id);
    kfree(connection);
    return OK;
}


static void _connection_close (res_container_t *c, int id, struct res_header *hdr)
{
    struct connection *connection = resm_get_ref(hdr);
    close_channel(connection->owner, connection->id);
}


void close_connections (struct process *proc)
{
    resm_container_free(proc->connections, _connection_close);
}
