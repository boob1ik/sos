#ifndef CHANNEL_H_
#define CHANNEL_H_

#include <os_types.h>
#include <syn/ksyn.h>
#include <rbtree.h>
#include "common/resm.h"
#include <syn/mutex.h>
#include "connection.h"
#include "pathname.h"

/** Кольцевой буфер */
struct rbuf {
    uint32_t size;
    uint8_t *head;
    uint8_t *tail;
    uint8_t *data;
};

/** Описатель канала */
struct channel {
    struct process *owner;
    int id;
    bool zombie;

    res_header_t *hdr; // ключ для удаления из контейнера процесса владельца
    pathname_t *pathname; // ключ для освобождения имени в пространстве имен
    enum channel_type type;
    unsigned long flags;

    int pathname_use;
    struct rb_tree connections;
    int msgs;
    struct rbuf buf;

    struct {
        struct thread *head;
        struct thread *tail;
    } receive;
    struct {
        struct thread *head;
        struct thread *tail;
    } send;
    struct {
        // потоки ожидающие установки соединения (os_channel_wait_connection)
        struct {
            struct thread *head;
            struct thread *tail;
        } wait;
        // потоки ожидающие установки соединения (os_connection_open)
        struct {
            struct thread *head;
            struct thread *tail;
        } open;
        // потоки ожидающие завершения установки соединения (os_channel_connection_complete)
        struct rb_tree complete;
    } connecting;
};

struct channel* lock_channel (struct process *proc, int id);
void unlock_channel (struct channel *channel);

struct channel * lock_channel_pathname (char *pathname, char **suffix);
//void unlock_channel_pathname (struct channel *channel);


/** \brief Открытие канала
 * \param pathname  Полный путь канала
 * \param size      Размер приемного буфера канала в байтах
 * \return Номер канала или ошибки исполнения
 *  */
int open_channel (struct thread *thr, channel_type_t type, char *pathname, size_t size, unsigned long flags);

/** \brief Закрытие канала.
 * \param id  Номер канала
 * \return Ошибки выполнения */
int close_channel (struct process *proc, int id);

/** \brief Ожидание соединения.
 * \param chid  Номер канала
 * \return Ошибки выполнения */
int channel_connection_wait (struct thread *thr, int chid, struct connection_info *info, uint64_t timeout);

/** \brief Завершение соединения.
 * \param chid  Номер канала
 * \return Ошибки выполнения */
int channel_connection_complete (struct thread *thr, int chid, enum connection_cmd cmd, int result);

void close_channels (struct process *proc);

void senders_unblock (struct channel *channel);

void zombie_channel (struct channel *channel);

#endif /* CHANNEL_H_ */
