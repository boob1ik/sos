#include "syn.h"
#include "ksyn.h"
#include "mutex.h"
#include <mem\kmem.h>
//#include <common\resmgr.h>
#include <common\resm.h>
#include <common\namespace.h>
#include <common\syshalt.h>
#include <limits.h>
#include <proc.h>
#include <sched.h>
//#include <os_types.h>

#include <stdlib.h>

#define SNFO_FLAG_UNLINKED      0x1
#define SNFO_FLAG_DELETED       0x2

/**
 * Менеджер объетов синхронизации строим на базе одного контейнера
 * объектов с диапазонами для статического и динамического захвата номера.
 */
/**
 * Различаем следующие типы объектов синхронизации:
 * 1) мьютексы и семафоры, рабочие счетчики которых находится в памяти процесса;
 * 2) мьютексы и семафоры, полностью размещенные в памяти ядра ОС;
 * 3) барьеры, которые также размещены в памяти ядра ОС.
 *
 * Первые являются локальными для процесса, однако могут быть разделяемыми, если
 * процесс разрешил доступ другим процессам к соответствующей странице своей памяти,
 * где размещена структура syn_t (см. ниже).
 * Эти мьютексы и семафоры относятся к классу быстрых, работа с ними может выполняться без
 * системных вызовов в отсутствии конкурентов, то есть при пустом списке ожидающих.
 * Минус их заключается в локальности для процесса и необходимости передачи прав доступа
 * к памяти другим процессам для межпроцессного взаимодействия. Плюсы -
 * быстрые мьютексы на коротких блоках синхронизации дают прирост в производительности
 * в 10 раз (4 постоянно активных потока с захватом, инкрементом переменной, освобождением)
 * и более(особенно при наличии потоков разных процессов), эффект снижается
 * чем реже в системе идет работа по блокам синхронизации и/или
 * чем больше время работы внутри блоков.
 *
 * Работа со вторыми ведется только через системные вызовы. Преимущества -
 * возможность межпроцессного взаимодействия, надежность.
 * Барьеры рассчитаны на блокировку текущего и ожидание других потоков,
 * поэтому работа с ними также выполняется только через системные вызовы.
 *
 */

struct synobj_header {
    struct namespace_node *rnn; // ссылка на объект в пространстве имен, если именованый
    struct process *owner;           // процесс-создатель (владелец) ресурса
    volatile int inuse_cnt;          // счетчик процессов-пользователей
    volatile int flags;
};

static kobject_lock_t syn_lock;
static res_container_t syn_storage;
static namespace_t syn_namespace;

void syn_allocator_init() {
    kobject_lock_init(&syn_lock);
    resm_container_init (&syn_storage, RES_CONTAINER_NUM_LIMIT_DEFAULT,
            RES_CONTAINER_MEM_LIMIT_DEFAULT, RES_ID_GEN_STRATEGY_INC_AGING);
    ns_init(&syn_namespace);
}

inline void syn_allocator_lock() {
    kobject_lock(&syn_lock);
}

inline void syn_allocator_unlock() {
    kobject_unlock(&syn_lock);
}

int syn_create(syn_t *s, struct process *p) {
    int resid = OK, tmp;
    struct res_header *reshdr, *resrefhdr;
    struct synobj_header *synhdr;
    size_t datalen = sizeof(struct synobj_header);
    struct namespace_node *rnn = NULL;

    if(s == NULL) {
        return ERR_ILLEGAL_ARGS;
    }
    switch(s->type) {
    case MUTEX_TYPE_PLOCAL:
    case MUTEX_TYPE_PSHARED:
        datalen += sizeof(mutex_t);
        break;
    case SEMAPHORE_TYPE_PLOCAL:
    case SEMAPHORE_TYPE_PSHARED:
        datalen += sizeof(semaphore_t);
        break;
    case BARRIER_TYPE_PLOCAL:
    case BARRIER_TYPE_PSHARED:
        datalen += sizeof(barrier_t);
        break;
    default:
        return ERR_ILLEGAL_ARGS;
    }
    if(s->pathname != NULL) {
        tmp = ns_node_create_and_lock(&syn_namespace, s->pathname, &rnn, KOBJECT_SYN);
        if(tmp != OK) {
            return tmp;
        }
        ns_node_unlock(&syn_namespace, rnn);
    } else {
        switch(s->type) {
        case MUTEX_TYPE_PSHARED:
        case SEMAPHORE_TYPE_PSHARED:
            return ERR_ILLEGAL_ARGS;
        default:
            break;
        }
    }
    resid = resm_create_and_lock(&syn_storage, RES_ID_GENERATE, datalen, &reshdr);
    if(resid <= 0) {
        if(rnn != NULL) {
            ns_node_delete(&syn_namespace, rnn, 0);
        }
        return resid;
    }
    if(rnn != NULL) {
        ns_node_set_value(rnn, resid); // в пространстве имен храним номер
    }
    reshdr->type = s->type;
    synhdr = (void *)reshdr + sizeof(struct res_header); // пользовательские данные - заголовок + объект синхронизации
    reshdr->ref = (void *)synhdr + sizeof(struct synobj_header); // сам объект расположен за reshdr и synhdr
    synhdr->flags = 0;
    synhdr->inuse_cnt = 1; // процесс-создатель сразу занимает объект для использования
    synhdr->owner = p;
    synhdr->rnn = rnn;

    switch(s->type) {
    case MUTEX_TYPE_PLOCAL:
    case MUTEX_TYPE_PSHARED:
        mutex_init((mutex_t *)reshdr->ref, s);
        break;
    case SEMAPHORE_TYPE_PLOCAL:
    case SEMAPHORE_TYPE_PSHARED:
        sem_init((semaphore_t *)reshdr->ref, s);
        break;
    case BARRIER_TYPE_PLOCAL:
    case BARRIER_TYPE_PSHARED:
        barrier_init((barrier_t *)reshdr->ref, s);
        break;
    }
    // добавляем ссылку в другое хранилище - объектов, созданных текущим процессом
    tmp = resm_create_and_lock(&p->syns, resid, 0, &resrefhdr);
    if(tmp != resid) {
        // ошибка
        syshalt(SYSHALT_OOPS_ERROR); // "p->syns consistency error on create"
    }
    resrefhdr->type = s->type;
    resrefhdr->ref = reshdr;
    resm_unlock(&p->syns, resrefhdr);
    // и в еще одно хранилище - доступных для данного процесса объектов синхронизации
    // так как делаем доступным сразу для процесса-создателя автоматически
    tmp = resm_create_and_lock(&p->syns_opened, resid, 0, &resrefhdr);
    if(tmp != resid) {
        // ошибка
        syshalt(SYSHALT_OOPS_ERROR); // "p->syns_opened consistency error on create"
    }
    resrefhdr->type = s->type;
    resrefhdr->ref = reshdr;
    resm_unlock(&p->syns_opened, resrefhdr);
    resm_unlock(&syn_storage, reshdr);
    return resid;
}

int syn_delete (int id, int unlink, struct process *p) {
    struct res_header *reshdr;
    struct synobj_header *synhdr;
    int res = ERR;
    int tmp;

    if(resm_search_and_lock(&syn_storage, id, &reshdr) != OK) {
        return res;
    }
    synhdr = (void *)reshdr + sizeof(struct res_header);
    if(synhdr->owner != p) {
        res = ERR_ACCESS_DENIED;
        resm_unlock(&syn_storage, reshdr);
        return res;
    }
    // из пространства имен можно сразу удалить, чтобы никто не смог открыть новый сеанс работы с объектом
    if(synhdr->rnn != NULL) {
        ns_node_delete(&syn_namespace, synhdr->rnn, 0);
    }
    // удаляем доступ с процесса владельца
    tmp = resm_search_remove(&p->syns_opened, id);
    if(tmp == OK) {
        if(synhdr->inuse_cnt == 0) {
            // ошибка
            syshalt(SYSHALT_OOPS_ERROR);//panic("snfo->inuse_cnt consistency error on delete");
        }
        synhdr->inuse_cnt--;
    }
    // удаляем из процесса владельца, объект ему не нужен больше
    tmp = resm_search_remove(&p->syns, id);
    if(tmp != OK) {
        // ошибка
        syshalt(SYSHALT_OOPS_ERROR);//panic("p->syns consistency error on delete");
    }

    if(unlink > 0) {
        synhdr->flags |= SNFO_FLAG_UNLINKED;
    } else {
        synhdr->flags |= SNFO_FLAG_DELETED;
        // Обязательно необходимо разбудить все потоки, блокированные на этом объекте синхронизации,
        // чтобы они не остались блокированными на бесконечность на зомбированном(не полностью удаленном объекте)
        // Внимание! мы не применяем методы хранения списка потоков, которые открыли данный ресурс для работы,
        // чтобы сразу полностью удалить объект синхронизации. Это не правильно даже с точки зрения возможного доступа
        // к памяти, если объект типа PLOCAL. Мы применяем отложенное удаление объекта при достижении счетчика 0,
        // что может произойти очень не скоро, до тех пор когда каждый процесс имеющий доступ к объекту не обратиться к
        // нему хотя бы один раз с любым системным вызовом. Поэтому зомби объект будет находится в памяти, хотя
        // в пространстве имен и у владельца он уже будет отсутствовать и на его месте может быть создан новый
        switch(reshdr->type) {
        case MUTEX_TYPE_PLOCAL:
        case MUTEX_TYPE_PSHARED:
            mutex_wait_cancel((mutex_t *)reshdr->ref, NULL);
            break;
        case SEMAPHORE_TYPE_PLOCAL:
        case SEMAPHORE_TYPE_PSHARED:
            sem_wait_cancel((semaphore_t *)reshdr->ref, NULL);
            break;
        case BARRIER_TYPE_PLOCAL:
        case BARRIER_TYPE_PSHARED:
            barrier_wait_cancel((barrier_t *)reshdr->ref, NULL);
            break;
        }
    }
    if(synhdr->inuse_cnt == 0) {
        // важно!!! освобождать память можно только после того как
        // счетчик использующих его процессов обнулится чтобы работали корректно syn_wait и syn_done
        resm_remove_locked(&syn_storage, reshdr);
    } else {
        resm_unlock(&syn_storage, reshdr);
    }
    return OK;
}

int syn_open (char *pathname, enum syn_type type, struct process *p) {
    struct namespace_node *rnn;
    struct res_header *reshdr, *resrefhdr;
    struct synobj_header *synhdr;
    int resid, tmp;

    resid = ns_node_search_and_lock(&syn_namespace, pathname, NULL, &rnn);
    if(resid != OK) {
        return resid;
    }
    if(ns_node_get_objtype(rnn) != KOBJECT_SYN) {
        ns_node_unlock(&syn_namespace, rnn);
        return ERR_ACCESS_DENIED;
    }
    resid = ns_node_get_value(rnn);
    if(resm_search_and_lock(&syn_storage, resid, &reshdr) != OK) {
        ns_node_unlock(&syn_namespace, rnn);
        return ERR_ACCESS_DENIED;
    }
    synhdr = (void *)reshdr + sizeof(struct res_header);
    if( (synhdr->flags & (SNFO_FLAG_DELETED | SNFO_FLAG_UNLINKED)) ||
            reshdr->type != type) {
        // объект был помечен как подлежащий удалению
        resm_unlock(&syn_storage, reshdr);
        ns_node_unlock(&syn_namespace, rnn);
        return ERR_ACCESS_DENIED;
    }
    switch(reshdr->type) {
    case MUTEX_TYPE_PLOCAL:
    case SEMAPHORE_TYPE_PLOCAL:
    case BARRIER_TYPE_PLOCAL:
        if(synhdr->owner != p) {
            // объект только для потоков внутри процесса
            resm_unlock(&syn_storage, reshdr);
            ns_node_unlock(&syn_namespace, rnn);
            return ERR_ACCESS_DENIED;
        }
        break;
    }
    tmp = resm_create_and_lock(&p->syns_opened, resid, 0, &resrefhdr);
    if(tmp == resid) {
        resrefhdr->type = type;
        resrefhdr->ref = reshdr;
        resm_unlock(&p->syns_opened, resrefhdr);
        synhdr->inuse_cnt++;
    } else if(tmp != ERR_BUSY) {
        syshalt(SYSHALT_OOPS_ERROR); // "p->syns_opened consistency error on open"
    }
    resm_unlock(&syn_storage, reshdr);
    ns_node_unlock(&syn_namespace, rnn);
    return resid;
}

int syn_close (int id, struct process *p) {
    struct res_header *reshdr;
    struct synobj_header *synhdr;
    int res = ERR;

    res = resm_search_remove(&p->syns_opened, id);
    if(res != OK) {
        return res;
    }
    res = resm_search_and_lock(&syn_storage, id, &reshdr);
    if(res != OK) {
        syshalt(SYSHALT_OOPS_ERROR);//panic("syn_storage consistency error on close");
    }
    synhdr = (void *)reshdr + sizeof(struct res_header);
    if(synhdr->inuse_cnt == 0) {
        // ошибка
        syshalt(SYSHALT_OOPS_ERROR);//panic("syn_storage consistency error on close");
    }
    synhdr->inuse_cnt--;
    if( (synhdr->inuse_cnt == 0) && (synhdr->flags & (SNFO_FLAG_UNLINKED | SNFO_FLAG_DELETED)) ) {
        // отложенное удаление, когда объект помечен на удаление владельцем до полного прекращения работы с ним
        res = resm_remove_locked(&syn_storage, reshdr);
        return res;
    }
    resm_unlock(&syn_storage, reshdr);
    return res;
}

int syn_wait (int id, struct thread *thr, uint64_t timeout) {
    struct res_header *reshdr, *resrefhdr;
    struct synobj_header *synhdr;
    int res = ERR;
    if(resm_search_and_lock(&thr->proc->syns_opened, id, &resrefhdr) != OK) {
        return res;
    }
    // можем сразу отпустить блокировку ссылочного объекта, так как он является своего рода разрешением на
    // выполнение действия в данный момент времени, жизненный цикл основного объекта им не управляется
    resm_unlock(&thr->proc->syns_opened, resrefhdr);
    reshdr = resrefhdr->ref;
    synhdr = (void *)reshdr + sizeof(struct res_header);
    if(synhdr->flags & SNFO_FLAG_DELETED) {
        // объект был помечен процессом-владельцем как подлежащий удалению
        return ERR_DEAD;
    }
    switch(reshdr->type) {
    case MUTEX_TYPE_PLOCAL:
    case MUTEX_TYPE_PSHARED:
        if(timeout == NO_WAIT) {
            res = mutex_trylock((mutex_t *)reshdr->ref, thr);
        } else {
            res = mutex_lock((mutex_t *)reshdr->ref, thr, timeout);
        }
        break;
    case SEMAPHORE_TYPE_PLOCAL:
    case SEMAPHORE_TYPE_PSHARED:
        if(timeout == NO_WAIT) {
            res = sem_trylock((semaphore_t *)reshdr->ref, thr);
        } else {
            res = sem_lock((semaphore_t *)reshdr->ref, thr, timeout);
        }
        break;
    case BARRIER_TYPE_PLOCAL:
    case BARRIER_TYPE_PSHARED:
        if(timeout == NO_WAIT) {
            res = ERR;
        } else {
            res = barrier_wait((barrier_t *)reshdr->ref, thr, timeout);
        }
        break;
    default:
        res = ERR;
    }
    if(synhdr->flags & SNFO_FLAG_DELETED) {
        // Объект был помечен процессом-владельцем как подлежащий удалению
        // Важно повторно проверить и вернуть ошибку, так как потоки могли быть заблокированы бесконечно
        // (без таймаутов) на ожидании синхронизации, а процедура удаления объекта синхронизации привела к их
        // разблокировке. Только здесь мы можем уточнить причину разблокировки для данного случая.
        // Синхронизация возможно не выполнена и должна быть возвращена соответствующая ошибка,
        // так как потоки могли быть разблокированы раньше завершения синхронизации. Даже если синхронизация
        // была выполнена и процедура удаления объекта инициирована чуть позже, возвращение ошибки будет корректным
        return ERR_DEAD;
    }
    return res;
}

int syn_done (int id, struct thread *thr) {
    struct res_header *reshdr, *resrefhdr;
    struct synobj_header *synhdr;
    int res = ERR;
    if(resm_search_and_lock(&thr->proc->syns_opened, id, &resrefhdr) != OK) {
        return res;
    }
    // можем сразу отпустить блокировку ссылочного объекта, так как он является своего рода разрешением на
    // выполнение действия в данный момент времени, жизненный цикл основного объекта им не управляется
    resm_unlock(&thr->proc->syns_opened, resrefhdr);
    reshdr = resrefhdr->ref;
    synhdr = (void *)reshdr + sizeof(struct res_header);
    if(synhdr->flags & SNFO_FLAG_DELETED) {
        // объект был помечен процессом-владельцем как подлежащий удалению
        return ERR_DEAD;
    }
    switch(reshdr->type) {
    case MUTEX_TYPE_PLOCAL:
    case MUTEX_TYPE_PSHARED:
        res = mutex_unlock((mutex_t *)reshdr->ref, thr);
        break;
    case SEMAPHORE_TYPE_PLOCAL:
    case SEMAPHORE_TYPE_PSHARED:
        res = sem_unlock((semaphore_t *)reshdr->ref, thr);
        break;
    case BARRIER_TYPE_PLOCAL:
    case BARRIER_TYPE_PSHARED:
        res = ERR; // для барьера действие не предусмотрено
        break;
    default:
        res = ERR;
    }
    return res;
}

static void syns_opened_finalize(res_container_t *container, int id, struct res_header *resrefhdr) {
    struct res_header *reshdr;
    struct synobj_header *synhdr;
//    resm_remove(container, resrefhdr);

    int res = resm_search_and_lock(&syn_storage, id, &reshdr);
    if(res != OK) {
        syshalt(SYSHALT_OOPS_ERROR);//panic("syn_storage consistency error on finalize");
    }
    synhdr = (void *)reshdr + sizeof(struct res_header);
    if(synhdr->inuse_cnt == 0) {
        // ошибка
        syshalt(SYSHALT_OOPS_ERROR);//panic("syn_storage consistency error on finalize");
    }
    synhdr->inuse_cnt--;
    if( (synhdr->inuse_cnt == 0) && (synhdr->flags & (SNFO_FLAG_UNLINKED | SNFO_FLAG_DELETED)) ) {
        // отложенное удаление, когда объект помечен на удаление владельцем до полного прекращения работы с ним
        res = resm_remove_locked(&syn_storage, reshdr);
        return;
    }
    resm_unlock(&syn_storage, reshdr);
}

static void syns_created_finalize(res_container_t *container, int id, struct res_header *resrefhdr) {
    struct res_header *reshdr;
    struct synobj_header *synhdr;
    int remove = 0;
//    resm_remove(container, resrefhdr);

    int res = resm_search_and_lock(&syn_storage, id, &reshdr);
    if(res != OK) {
        syshalt(SYSHALT_OOPS_ERROR);//panic("syn_storage consistency error on finalize");
    }
    synhdr = (void *)reshdr + sizeof(struct res_header);
    // из пространства имен можно сразу удалить, чтобы никто не смог открыть новый сеанс работы с объектом
    if(synhdr->rnn != NULL) {
        ns_node_delete(&syn_namespace, synhdr->rnn, 0);
    }
    synhdr->flags |= SNFO_FLAG_DELETED;
    if(synhdr->inuse_cnt == 0) {
        remove = 1;
    }
    resm_unlock(&syn_storage, reshdr);

    // Обязательно необходимо разбудить все потоки, блокированные на этом объекте синхронизации,
    // чтобы они не остались блокированными на бесконечность на зомбированном(не полностью удаленном объекте)
    // Внимание! мы не применяем методы хранения списка потоков, которые открыли данный ресурс для работы,
    // чтобы сразу полностью удалить объект синхронизации. Это не правильно даже с точки зрения возможного доступа
    // к памяти, если объект типа PLOCAL. Мы применяем отложенное удаление объекта при достижении счетчика 0,
    // что может произойти очень не скоро, до тех пор когда каждый процесс имеющий доступ к объекту не обратиться к
    // нему хотя бы один раз с любым системным вызовом. Поэтому зомби объект будет находится в памяти, хотя
    // в пространстве имен и у владельца он уже будет отсутствовать и на его месте может быть создан новый
    switch(reshdr->type) {
    case MUTEX_TYPE_PLOCAL:
    case MUTEX_TYPE_PSHARED:
        mutex_wait_cancel((mutex_t *)reshdr->ref, NULL);
        break;
    case SEMAPHORE_TYPE_PLOCAL:
    case SEMAPHORE_TYPE_PSHARED:
        sem_wait_cancel((semaphore_t *)reshdr->ref, NULL);
        break;
    case BARRIER_TYPE_PLOCAL:
    case BARRIER_TYPE_PSHARED:
        barrier_wait_cancel((barrier_t *)reshdr->ref, NULL);
        break;
    }

    if(remove) {
        resm_remove(&syn_storage, reshdr);
    }
}

void syn_proc_finalize(struct process *p) {
    resm_container_free(&p->syns_opened, syns_opened_finalize);
    resm_container_free(&p->syns, syns_created_finalize);
}
