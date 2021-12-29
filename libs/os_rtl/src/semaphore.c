#include <os-libc.h>
#include <stdlib.h>
#include <stdarg.h>
#include <fcntl.h>
#include <semaphore.h>
#include "plocal_sem.h"
/**
 * Модуль управления семафорами, которые относятся к функционалу
 * стандартного API с максимальным приближением к стандарту POSIX 1003.1.
 *
 * Работу с дескрипторами по семафорам выполняем без дополнительных механизмов
 * запоминания информации о созданных объектах для данного процесса.
 * Предусматриваем постоянное хранение объекта syn_t после его создания здесь в библиотеке,
 * до вызова соответсвующих методов удаления.
 * Дескриптор семафора - это адрес объекта syn_t в адресном пространстве процесса.
 * По этому адресу сразу выполняем доступ к полям и получаем id номер объекта полученный от ядра,
 * тип объекта, рабочие поля для локальных быстрых семафоров.
 * Любые ошибки в значении дескрипторов на входе могут повлечь за собой непредвиденное поведение процесса,
 * однако в конечном счете мы имеем защищенные MMU процессы и возможны только два варианта:
 * - обращение к недоступной памяти, процесс удаляется системой по ошибке;
 * - обращение к доступной памяти, процесс(все его потоки) могут начать не адекватно себя вести в рамках
 *  доступной памяти и функционала. Последний случай - нормальная ситуация, которая может возникнуть при
 *  внесенных ошибках в процессе разработки. Считаем что ошибка в дескрипторе и последствия - отказ одного уровня
 *  в рамках одного процесса и его взаимодействия с другими процессами.
 */

//! < Признак открытого семафора который был создан другим процессом, используем поле limit чтобы не вводить другое
#define SEM_LIMIT_OPENED        -1

int sem_init (sem_t * sem, int pshared, unsigned value)
{
    if(pshared != 0) {
        errno = ENOTSUP;
        return -1;
    }
    syn_t *synobj = sem;
    if(synobj == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(synobj->id > 0) {
        errno = EBUSY;
        return -1;
    }

    synobj->id = 0;
    synobj->limit = value;
    synobj->pathname = NULL;
    synobj->type = SEMAPHORE_TYPE_PLOCAL;
    int res = os_syn_create(synobj);
    if(res > 0) {
        return OK;
    }
    switch(res) {
    case ERR_NO_MEM:
        errno = ENOSPC;
        break;
    default:
    case ERR:
        errno = EINVAL;
        break;
    }
    return -1;
}

int sem_destroy (sem_t * sem)
{
    int res;
    syn_t *synobj = sem;
    if(synobj->type != SEMAPHORE_TYPE_PLOCAL) {
        errno = EINVAL;
        return -1;
    }
    res = os_syn_delete(synobj->id, 0);
    if(res != OK) {
        errno = EINVAL;
        return -1;
    }
    synobj->id = 0;
    return OK;
}

sem_t * sem_open (const char * sem_name, int oflags, ...)
{
    va_list vl;
    mode_t mode;
    unsigned int val;
    if(sem_name == NULL) {
        errno = EINVAL;
        return SEM_FAILED;
    }
    if(oflags & O_CREAT) {
        va_start(vl, oflags);
        mode = va_arg(vl, mode_t);
        val = va_arg(vl, unsigned int);
        va_end(vl);
        if(val > INT_MAX) {
            errno = EINVAL;
            return SEM_FAILED;
        }
    }

    syn_t *synobj = (syn_t *)malloc(sizeof(syn_t));
    if(synobj == NULL) {
        errno = ENOSPC;
        return SEM_FAILED;
    }
    synobj->pathname = (char *)sem_name;
    synobj->type = SEMAPHORE_TYPE_PSHARED;
    int res = 0;

    if(oflags & O_CREAT) {
        synobj->limit = val;
        res = os_syn_create(synobj);
        if(res > 0) {
            return synobj;
        }
        switch(res) {
        case ERR_ILLEGAL_ARGS:
            errno = EINVAL;
            free(synobj);
            return SEM_FAILED;
        case ERR_PATHNAME_TOO_LONG:
            errno = ENAMETOOLONG;
            free(synobj);
            return SEM_FAILED;
        case ERR_BUSY:
            if((oflags & O_EXCL) == 0) {
                res = os_syn_open(sem_name, SEMAPHORE_TYPE_PSHARED);
                if(res > 0) {
                    synobj->limit = SEM_LIMIT_OPENED;
                    synobj->id = res;
                    return synobj;
                }
                errno = EACCES;
            } else {
                errno = EEXIST;
            }
            free(synobj);
            return SEM_FAILED;

        default:
        case ERR_ACCESS_DENIED:
            free(synobj);
            errno = EACCES;
            return SEM_FAILED;

        }
    }
    res = os_syn_open(sem_name, SEMAPHORE_TYPE_PSHARED);
    if(res > 0) {
        synobj->limit = SEM_LIMIT_OPENED;
        synobj->id = res;
        return synobj;
    }
    free(synobj);
    errno = EACCES;
    return SEM_FAILED;
}

int sem_close (sem_t * sem)
{
    if( (sem == NULL) || (sem->type != SEMAPHORE_TYPE_PSHARED) ) {
        errno = EINVAL;
        return -1;
    }
    int res;
    if(sem->limit == SEM_LIMIT_OPENED) {
        res = os_syn_close(sem->id);
        return ((res == OK)? res : EINVAL);
    }
    res = os_syn_delete(sem->id, 0);
    if(res == OK) {
        return OK;
    }
    errno = EINVAL;
    return -1;
}

/**
 * Данная функция является прямым аналогом стандартной sem_unlink за исключением того что удаление
 * проводится по идентификатору, а не по пути с именем. Только владелец имеет право вызвать данную функцию
 * @param sem
 * @return
 */
int sem_unlink_destroy (sem_t * sem)
{
    if( (sem == NULL) || (sem->type != SEMAPHORE_TYPE_PSHARED) ) {
        errno = EINVAL;
        return -1;
    }
    int res;
    if(sem->limit == SEM_LIMIT_OPENED) {
        // открытые на доступ семафоры просто закрываем
        res = os_syn_close(sem->id);
        return ((res == OK)? res : EINVAL);
    }
    // созданные данным процессом семафоры удаляем в отложенном режиме, как предполагается по стандарту POSIX
    res = os_syn_delete(sem->id, 1);
    if(res == OK) {
        return OK;
    }
    errno = EINVAL;
    return -1;
}

int sem_trywait (sem_t * sem)
{
    int res;
    if(sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(sem->type == SEMAPHORE_TYPE_PSHARED) {
        res = os_syn_wait(sem->id, NO_WAIT);
    } else {
        res = plocal_sem_wait(sem, NO_WAIT);
    }
    switch(res) {
    case OK:
        return res;
    }
    errno = EAGAIN;
    return -1;
}

int sem_wait (sem_t * sem)
{
    int res;
    if(sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(sem->type == SEMAPHORE_TYPE_PSHARED) {
        res = os_syn_wait(sem->id, TIMEOUT_INFINITY);
    } else {
        res = plocal_sem_wait(sem, TIMEOUT_INFINITY);
    }
    switch(res) {
    case OK:
        return res;
    }
    errno = EINTR;
    return -1;
}

int sem_timedwait (sem_t * sem, const struct timespec * abs_timeout)
{
    int res;
    if(sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(sem->type == SEMAPHORE_TYPE_PSHARED) {
        res = os_syn_wait(sem->id, TIMEOUT_INFINITY);
    } else {
        res = plocal_sem_wait(sem, TIMEOUT_INFINITY);
    }
    switch(res) {
    case OK:
        return res;
    }
    errno = ETIMEDOUT;
    return -1;
}

int sem_post (sem_t * sem)
{
    int res;
    if(sem == NULL) {
        errno = EINVAL;
        return -1;
    }
    if(sem->type == SEMAPHORE_TYPE_PSHARED) {
        res = os_syn_done(sem->id);
    } else {
        res = plocal_sem_done(sem);
    }
    return OK;
}

