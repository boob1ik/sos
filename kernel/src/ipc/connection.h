#ifndef CONNECTION_H_
#define CONNECTION_H_

#include <os_types.h>
#include <syn/ksyn.h>
#include "common\resm.h"

/** данные для процедуры установки соединения */
struct connecting {
    int err;
    struct channel *channel;
    uint64_t timeout;
    struct thread *thr;
    int reply;
    struct connection_info *info;
};

/** Описатель соединения */
struct connection {
    struct process *owner;
    int id;

    struct channel *channel;
    struct connection *reply;
    res_header_t *hdr;

    struct connecting *connecting;
};

struct connection* lock_connection (struct process *proc, int id);
void unlock_connection (struct connection *connection);

/** \brief Открыть соединение
 *
 * \param chid       Номер канала
 * \param name       Имя канала
 * \param ch_reply   Канал для ответов (0 - без ответа, однонаправленное соединение)
 *
 * \return Номер соединения или ошибки исполнения*/
int open_connection (struct thread *thr, char *pathname, uint64_t timeout, int reply_chid);

/** \brief Закрыть соединение
 *
 * \param id Номер соединения
 *
 * \return Ошибки исполнения */
int close_connection (struct process *proc, int id);

void connect (struct channel *channel, struct connection *connection);
void disconnect_low (struct connection *connection);

void close_connections (struct process *proc);

int redirect_connecting (struct process *proc, struct connection *connection);
int reply_connect (struct process *proc, struct connection *connection);

#endif /* CONNECTION_H_ */
