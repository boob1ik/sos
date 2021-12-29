#include <arch.h>
#include <rbtree64.h>
#include "common/syshalt.h"
#include "mem/kmem.h"
#include "syn\ksyn.h"
#include "thread.h"

struct kevent {
    struct kevent *prev;    // делаем замкнутый список чтобы иметь доступ к началу и концу
    struct kevent *next;    // организуем таким образом FIFO событий по одной метке времени
    struct rb_node64 *node; // для очистки памяти при удалении события по ссылке на событие
    struct thread *thr;     // суть события, источник всегда поток
};

static inline void kevent_dlist_insert (struct kevent *prev, struct kevent *node)
{
    node->prev = prev;
    node->next = prev->next;
//    if (prev->next != NULL)
    prev->next->prev = node;
    prev->next = node;
}

static inline void kevent_dlist_remove (struct kevent *node)
{
//    if (node->prev != NULL)
        node->prev->next = node->next;
//    if (node->next != NULL)
        node->next->prev = node->prev;
}

static struct rb_tree64 *kevents;   // дерево событий
static struct rb_node64 *time_node; // текущее ближайшее событие (наступившее или нет)
// по наступившему событию не пустые ссылки:
static struct kevent *first;  // первый объект двусвязного замкнутого списка для скорости доступа
static struct kevent *current;// следующий объект, который должен быть выбран по kevent_fetch
static kobject_lock_t elock;

void kevent_init() {
    kevents = (struct rb_tree64 *) kmalloc(sizeof(struct rb_tree64));
    rb_tree64_init(kevents);
    time_node = NULL;
    current = NULL;
    kobject_lock_init(&elock);
}

void kevent_store_lock() {
    kobject_lock(&elock);
}

void kevent_store_unlock() {
    kobject_unlock(&elock);
}

void *kevent_insert(uint64_t event_time_ns, struct thread *thr) {

    struct kevent *evt = (struct kevent *) kmalloc(sizeof(struct kevent));
    evt->next = evt;
    evt->prev = evt;
    evt->thr = thr;
    struct rb_node64 *node = rb_tree64_search(kevents, event_time_ns);
    if(node == NULL) {
        node = (struct rb_node64 *) kmalloc(sizeof(struct rb_node64));
        evt->node = node;
        rb_node64_init(node);
        rb_node64_set_key(node, event_time_ns);
        rb_node64_set_data(node, evt);
        rb_tree64_insert(kevents, node);
        if(current == NULL) {
            time_node = rb_tree64_get_min(kevents);
        }
    } else {
        struct kevent *old = rb_node64_get_data(node);
        evt->node = node;
        if(old != NULL) {
            kevent_dlist_insert(old->prev, evt);
        } else {
            syshalt(SYSHALT_OOPS_ERROR);//panic("NULL kevent list");
        }
    }
    return evt;
}

/**
 * Внимание! Функция отмены предполагает нахождение события в менеджере событий
 * в момент вызова. Внешний код должен гарантировать, что после выборки события по kevent_fetch
 * метод kevent_cancel по выбранному событию не будет вызван.
 */
void kevent_cancel(void *e) {
    if(e == NULL) {
        return;
    }
    struct kevent *evt = (struct kevent *)e;
    kevent_store_lock();
    if(current == evt) {
        // специальный случай, когда событие уже наступило но не выбрано и
        // следующее на очереди именно указанное событие, его тоже нужно отменить
        if(current->next == first) {
            //следующего в списке по данному времени(узлу time_node) нет, можем удалить и обновить ожидаемое событие
            current = NULL;
            kfree(time_node);
            time_node = rb_tree64_get_min(kevents);
        } else {
            current = current->next;
        }
    } else {
        kevent_dlist_remove(evt);
        if( (evt->prev == evt) && (evt->next == evt) ) {
            // по данному времени(узлу evt->node) нет других событий
            if(evt->node != time_node) {
                // узел не являлся ближайшим ожидаемым, удаляем его из дерева событий
                rb_tree64_remove(kevents, evt->node);
                kfree(evt->node);
            } else if(current != NULL) {
                // этого условия никогда не должно возникнуть, это равносильно current == evt
                current = NULL;
                kfree(time_node);
                time_node = rb_tree64_get_min(kevents);
            }
        }
    }
    kfree(e);
    kevent_store_unlock();
}

int kevent_get_time(uint64_t *time) {
    if(current != NULL) {
        *time = rb_node64_get_key(current->node);
        return 0;
    }
    return -1;
}


/**
 * Внимание! Для корректной выборки события и недопущения вызова после этого kevent_cancel
 * по указанному событию необходимо внешним кодом с синхронизацией по kevent_store_lock
 * удалить все упоминания извне о зарегистрированном событии.
 */

struct thread *kevent_fetch() {
    kevent_store_lock();
    if(time_node == NULL) {
        // дерево событий в этом случае всегда пустое
        return NULL;
    }
    if( (current == NULL) && (systime() >= rb_node64_get_key(time_node)) ) {
        // наступило событие, начинаем выбирать объекты из списка
        current = rb_node64_get_data(time_node);
        first = current;
        // удаляем сразу список объектов из дерева событий чтобы вставка в дерево не повлияла
        // на процесс выборки объектов из списка по одному, память пока не освобождаем
        rb_tree64_remove(kevents, time_node);
    }
    struct thread *thr = NULL;
    if(current != NULL) {
        struct kevent *next = current->next;
        thr = current->thr;
        kfree((void *)current);
        if(next == first) {
            current = NULL;
            kfree(time_node);
            time_node = rb_tree64_get_min(kevents);
        } else {
            current = next;
        }
    }
    kevent_store_unlock();
    return thr;
}

