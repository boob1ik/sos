#include <os-libc.h>
#include <stdlib.h>
#include <rbtree.h>
#include "plocal_mutex.h"

#define MUTEX_ATTR_MAGIC_INIT       0x5511
#define BARRIER_ATTR_MAGIC_INIT     0x6622

/**
 * Модуль управления объктами синхронизации для пользователя, которые относятся к функционалу
 * стандартного API pthread с максимальным приближением к стандарту POSIX 1003.1.
 *
 * Мьютексы типа PTHREAD_PROCESS_PRIVATE соответсвуют типу в ядре ОС MUTEX_TYPE_PLOCAL и являются
 * быстрыми, то есть их реализация предполагает отсутствие системных вызовов при отсутствии ожидающих потоков.
 *
 * Мьютексы типа PTHREAD_PROCESS_SHARED соответсвуют ядерныму типу MUTEX_TYPE_PSHARED и работа с ними
 * ведется всегда по системным вызовам. Несмотря на то, что ОС предусматривает возможность синхронизации разных
 * процессов между собой быстрыми мьютексами MUTEX_TYPE_PLOCAL, библиотека эту возможность не использует.
 * Это связано с необходимостью передачи доступа к области памяти объекта syn_t владельца другим процессам,
 * что может и должно быть сделано только в явном виде самим разработчиком.
 * ОС поддерживает только PTHREAD_PROCESS_SHARED барьеры, они аналогичны таким же мьютексам.
 *
 * Работу с дескрипторами по объектам синхронизации выполняем без дополнительных механизмов
 * запоминания информации о созданных объектах для данного процесса.
 * Предусматриваем постоянное хранение объекта syn_t после его создания здесь в библиотеке,
 * до вызова соответсвующих методов удаления.
 * Дескриптор мьютекса или барьера - это адрес объекта syn_t в адресном пространстве процесса.
 * По этому адресу сразу выполняем доступ к полям и получаем id номер объекта полученный от ядра,
 * тип объекта, рабочие поля для локальных быстрых типов.
 * Любые ошибки в значении дескрипторов на входе могут повлечь за собой непредвиденное поведение процесса,
 * однако в конечном счете мы имеем защищенные MMU процессы и возможны только два варианта:
 * - обращение к недоступной памяти, процесс удаляется системой по ошибке;
 * - обращение к доступной памяти, процесс(все его потоки) могут начать не адекватно себя вести в рамках
 *  доступной памяти и функционала. Последний случай - нормальная ситуация, которая может возникнуть при
 *  внесенных ошибках в процессе разработки. Считаем что ошибка в дескрипторе и последствия - отказ одного уровня
 *  в рамках одного процесса и его взаимодействия с другими процессами.
 */


/*
static struct rb_tree *pstore_synobjects = NULL;
static syn_t *pstore_syn = NULL;
static int plocal_store_init() {
    int res;
    struct rb_tree *tree = malloc(sizeof(struct rb_tree));
    pstore_syn = malloc(sizeof(syn_t));
    if(tree == NULL) {
        return ENOMEM;
    }
    if(pstore_syn == NULL) {
        free(tree);
        return ENOMEM;
    }
    rb_tree_init(tree);
    pstore_syn->pathname = NULL;
    pstore_syn->type = MUTEX_TYPE_PLOCAL;
    res = os_syn_create(pstore_syn);
    if(res < 0) {
        free(tree);
        free(pstore_syn);
        return ENOMEM; // только отсутствие памяти может быть причиной отказа в регистрации такого мьютекса ядром
    }
    pstore_synobjects = tree;
    return OK;
}

static int pstore_synobj_save(syn_t *synobj) {
    if(pstore_synobjects == NULL) {
        plocal_store_init();
    }
    struct rb_node *n = malloc(sizeof(struct rb_node));
    if(n == NULL) {
        return ENOMEM;
    }
    plocal_mutex_wait (pstore_syn, TIMEOUT_INFINITY);
    rb_node_set_key(n, (size_t)synobj);
    rb_tree_insert(pstore_synobjects, n);
    plocal_mutex_done(pstore_syn);
    return OK;
}

static int pstore_synobj_delete(syn_t *synobj) {
    plocal_mutex_wait (pstore_syn, TIMEOUT_INFINITY);
    struct rb_node *n = rb_tree_search(pstore_synobjects, (size_t)synobj);
    if(n != NULL) {
        rb_tree_remove(pstore_synobjects, n);
        free(n);
    }
    plocal_mutex_done(pstore_syn);
    return OK;
}
*/
int pthread_mutexattr_init (pthread_mutexattr_t *__attr)
{
    __attr->type = PTHREAD_MUTEX_DEFAULT;
    __attr->recursive = 0;
    __attr->protocol = PTHREAD_PRIO_NONE;
    __attr->process_shared = PTHREAD_PROCESS_PRIVATE;
    __attr->prio_ceiling = PRIO_MAX;
    __attr->is_initialized = MUTEX_ATTR_MAGIC_INIT;
    return OK;
}

int pthread_mutexattr_destroy (pthread_mutexattr_t *__attr)
{
    __attr->is_initialized = 0;
    return OK;
}

int pthread_mutexattr_getpshared (_CONST pthread_mutexattr_t *__attr, int *__pshared)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    *__pshared = __attr->process_shared;
    return OK;
}

int pthread_mutexattr_setpshared (pthread_mutexattr_t *__attr, int __pshared)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    switch(__pshared) {
    case PTHREAD_PROCESS_PRIVATE:
    case PTHREAD_PROCESS_SHARED:
        __attr->process_shared = __pshared;
        break;
    default:
        return EINVAL;
    }
    return OK;
}

int pthread_mutexattr_gettype (_CONST pthread_mutexattr_t *__attr, int *__type)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    *__type = __attr->type;
    return OK;
}

int pthread_mutexattr_settype (pthread_mutexattr_t *__attr, int __type)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    switch(__type) {
    case PTHREAD_MUTEX_NORMAL:
    case PTHREAD_MUTEX_RECURSIVE:
    case PTHREAD_MUTEX_ERRORCHECK:
        __attr->type = __type;
        break;
    case PTHREAD_MUTEX_DEFAULT:
        __attr->type = PTHREAD_MUTEX_ERRORCHECK;
        break;
    default:
        return EINVAL;
    }
    return OK;
}

int pthread_mutexattr_getprotocol (_CONST pthread_mutexattr_t *__attr, int *__protocol)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    *__protocol = __attr->protocol;
    return OK;
}

int pthread_mutexattr_setprotocol (pthread_mutexattr_t *__attr, int __protocol)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    switch(__protocol) {
    case PTHREAD_PRIO_NONE:
    case PTHREAD_PRIO_INHERIT:
    case PTHREAD_PRIO_PROTECT:
        __attr->protocol = __protocol;
        break;
    default:
        return EINVAL;
    }
    return OK;
}

int pthread_mutexattr_getprioceiling (_CONST pthread_mutexattr_t *__attr, int *__prioceiling)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    *__prioceiling = __attr->prio_ceiling;
    return OK;
}

int pthread_mutexattr_setprioceiling (pthread_mutexattr_t *__attr, int __prioceiling)
{
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    if(__prioceiling < PRIO_MAX || __prioceiling > PRIO_MIN) {
        return EINVAL;
    }
    __attr->prio_ceiling = __prioceiling;
    return OK;
}

int pthread_named_mutex_init (pthread_mutex_t *__mutex, _CONST pthread_mutexattr_t *__attr, char *pathname)
{
    int res;
    syn_t *synobj;
    if(__attr->is_initialized != MUTEX_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    synobj = malloc(sizeof(syn_t));
    synobj->pathname = pathname;
    switch(__attr->process_shared) {
    case PTHREAD_PROCESS_PRIVATE:
        synobj->type = MUTEX_TYPE_PLOCAL;
        break;
    case PTHREAD_PROCESS_SHARED:
        synobj->type = MUTEX_TYPE_PSHARED;
        break;
    default:
        free(synobj);
        return EINVAL;
    }
    res = os_syn_create(synobj);
    if(res > 0) {
        *__mutex = (pthread_mutex_t)synobj; // идентификатор мьютекса будет = &synobj, необходима работа в потоке и ядре
    } else {
        *__mutex = 0;
        free(synobj);
        return ENOMEM;
    }
    return OK;
}

int pthread_mutex_init (pthread_mutex_t *__mutex, _CONST pthread_mutexattr_t *__attr)
{
    return pthread_named_mutex_init(__mutex, __attr, NULL);
}

int pthread_mutex_destroy (pthread_mutex_t *__mutex)
{
    syn_t *synobj = (syn_t *)*__mutex;
//    pstore_synobj_delete(synobj);
    if(os_syn_delete(synobj->id,0) != OK) {
        return EINVAL;
    }
    synobj->id = 0;
    free(synobj);
    return OK;
}

int pthread_mutex_lock (pthread_mutex_t *__mutex)
{
    int res;
    syn_t *synobj = (syn_t *)*__mutex;
    if(synobj->id > 0) {
        if(synobj->type == MUTEX_TYPE_PLOCAL) {
            res = plocal_mutex_wait(synobj, TIMEOUT_INFINITY);
        } else {
            res = os_syn_wait(synobj->id, TIMEOUT_INFINITY);
        }
        if(res == OK) {
            return res;
        }
    }
    return EINVAL;
}

int pthread_mutex_trylock (pthread_mutex_t *__mutex)
{
    int res;
    syn_t *synobj = (syn_t *)*__mutex;
    if(synobj->id > 0) {
        if(synobj->type == MUTEX_TYPE_PLOCAL) {
            res = plocal_mutex_wait(synobj, NO_WAIT);
        } else {
            res = os_syn_wait(synobj->id, NO_WAIT);
        }
        if(res == OK) {
            return res;
        } else if(res == ERR_BUSY) {
            return EBUSY;
        }
    }
    return EINVAL;
}

int pthread_mutex_unlock (pthread_mutex_t *__mutex)
{
    int res;
    syn_t *synobj = (syn_t *)*__mutex;
    if(synobj->id > 0) {
        if(synobj->type == MUTEX_TYPE_PLOCAL) {
            res = plocal_mutex_done(synobj);
        } else {
            res = os_syn_done(synobj->id);
        }
        if(res == OK) {
            return res;
        }
    }
    return EINVAL;
}

int pthread_mutex_timedlock (pthread_mutex_t *__mutex, _CONST struct timespec *__timeout)
{
    int res;
    uint64_t ns;
    syn_t *synobj = (syn_t *)*__mutex;
    if(__timeout->tv_sec > 0) {
        ns = __timeout->tv_sec * 1000000000 + __timeout->tv_nsec;
    } else {
        ns = __timeout->tv_nsec;
    }
    if(synobj->id > 0) {
        if(synobj->type == MUTEX_TYPE_PLOCAL) {
            res = plocal_mutex_wait(synobj, ns);
        } else {
            res = os_syn_wait(synobj->id, ns);
        }
        if(res == OK) {
            return res;
        } else if(res == ERR_TIMEOUT) {
            return ETIME;
        }
    }
    return EINVAL;
}

int pthread_barrierattr_init (pthread_barrierattr_t *__attr)
{
    __attr->process_shared = PTHREAD_PROCESS_PRIVATE;
    __attr->is_initialized = BARRIER_ATTR_MAGIC_INIT;
    return OK;
}

int pthread_barrierattr_destroy (pthread_barrierattr_t *__attr)
{
    __attr->is_initialized = 0;
    return OK;
}

int pthread_barrierattr_getpshared (_CONST pthread_barrierattr_t *__attr, int *__pshared)
{
    if(__attr->is_initialized != BARRIER_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    *__pshared = __attr->process_shared;
    return OK;
}

int pthread_barrierattr_setpshared (pthread_barrierattr_t *__attr, int __pshared)
{
    if(__attr->is_initialized != BARRIER_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    switch(__pshared) {
    case PTHREAD_PROCESS_PRIVATE:
    case PTHREAD_PROCESS_SHARED:
        __attr->process_shared = __pshared;
        break;
    default:
        return EINVAL;
    }
    return OK;
}

#define PTHREAD_BARRIER_SERIAL_THREAD       -1

int pthread_named_barrier_init (pthread_barrier_t *__barrier, _CONST pthread_barrierattr_t *__attr,
        unsigned __count, char *pathname)
{
    int res;
    syn_t synobj;
    if(__attr->is_initialized != BARRIER_ATTR_MAGIC_INIT) {
        return EINVAL;
    }
    synobj.pathname = pathname;
    switch(__attr->process_shared) {
    case PTHREAD_PROCESS_PRIVATE:
        synobj.type = BARRIER_TYPE_PLOCAL;
        break;
    case PTHREAD_PROCESS_SHARED:
        synobj.type = BARRIER_TYPE_PSHARED;
        break;
    default:
        return EINVAL;
    }
    synobj.limit = __count;
    res = os_syn_create(&synobj);
    if(res > 0) {
        *__barrier = synobj.id; // идентификатор барьеров будет = id, работа с ним только в микроядре ОС
    } else {
        *__barrier = 0;
        return ENOMEM;
    }
    return OK;
}

int pthread_barrier_init (pthread_barrier_t *__barrier, _CONST pthread_barrierattr_t *__attr, unsigned __count)
{
    return pthread_named_barrier_init(__barrier, __attr, __count, NULL);
}

int pthread_barrier_destroy (pthread_barrier_t *__barrier)
{
    if(os_syn_delete(*__barrier,0) != OK) {
        return EINVAL;
    }
    return OK;
}

int pthread_barrier_wait (pthread_barrier_t *__barrier)
{
    int res = os_syn_wait(*__barrier, TIMEOUT_INFINITY);
    if(res == ERR_TIMEOUT) {
        return ETIME;
    }
    if(res == OK) {
        return OK;
    } else {
        return EINVAL;
    }
}
