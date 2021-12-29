#include <arch.h>
#include "resm.h"
#include <rbtree.h>
#include <mem/kmem.h>
#include <syn/ksyn.h>
#include <common/syshalt.h>
#include <string.h>


typedef struct inner_res_container {
    kobject_lock_t lock;
    int finalizing;
    int numlimit;
    size_t memlimit;
    size_t memused;
    int gen_strategy;

    struct rb_tree objects;
    struct rb_tree inverted_free_ids[2];
    int current_free_subspace;
} inner_res_container_t;

typedef struct res_node_header {
    kobject_lock_t lock;
    struct res_header user_header;
} res_node_header_t;

#define RES_ID_INVERT(id, numlimit) (numlimit - id + 1)

#define RES_NODE_BASE_SIZE    (sizeof(struct rb_node) + sizeof(struct res_node_header))

int resm_container_init (res_container_t *c, int numlimit, size_t memlimit, gen_strategy_t gen_strategy)
{

    if ((numlimit < RES_CONTAINER_NUM_LIMIT_MIN) || (numlimit > RES_CONTAINER_NUM_LIMIT_MAX)) {
        return ERR_ILLEGAL_ARGS;
    }

    inner_res_container_t *container = (inner_res_container_t *) kmalloc(sizeof(inner_res_container_t));
    container->numlimit = numlimit;
    container->memlimit = memlimit;
    container->memused = 0;
    container->gen_strategy = gen_strategy;
    container->finalizing = 0;

    kobject_lock_init(&container->lock);
    rb_tree_init(&container->objects);
    rb_tree_init(&container->inverted_free_ids[0]);
    rb_tree_init(&container->inverted_free_ids[1]);
    container->current_free_subspace = 0;

    struct rb_node *node = (struct rb_node *) kmalloc(sizeof(struct rb_node));
    rb_node_init(node);
    rb_node_set_key(node, 1);
    rb_node_set_data(node, (void *) (numlimit));
    rb_tree_insert(&container->inverted_free_ids[container->current_free_subspace], node);

    *c = container;
    return OK;
}

int resm_create_and_lock (res_container_t *c, int id, size_t len, struct res_header **hdr)
{
    struct rb_node *node;
    inner_res_container_t *container = *c;
    res_node_header_t *inner_hdr;
    size_t memlen;
    if ((container == NULL) || (hdr == NULL))
        return ERR_ILLEGAL_ARGS;
    if ((id != RES_ID_GENERATE) && (container->gen_strategy != RES_ID_GEN_STRATEGY_NOGEN)) {
        return ERR_ILLEGAL_ARGS;
    }
    if(container->finalizing) {
        // рекурсивный вызов из res_free недопустим
        syshalt(SYSHALT_RESM_RECURSION_ERROR);
        return 0;
    }
    kobject_lock(&container->lock);
    if ((container->memlimit != RES_CONTAINER_MEM_UNLIMITED) && (container->memused >= container->memlimit)) {
        kobject_unlock(&container->lock);
        return ERR_NO_MEM;
    }

    if (id == RES_ID_GENERATE) {
        // динамический захват любого свободного номера,
        // выполняем отсечением свободного диапазона справа,
        // так как не нужно удалять узел из памяти и выполнять
        // соответствующие дополнительные удаления/вставки
        node = rb_tree_get_max(&container->inverted_free_ids[container->current_free_subspace]);
        if (node == NULL) {
            // переполнение поиска, делаем циклический переход на другое подпространство свободных номеров
            container->current_free_subspace ^= 1;
            node = rb_tree_get_max(&container->inverted_free_ids[container->current_free_subspace]);
        }
        if (node == NULL) {
            // контейнер переполнен, нет свободных номеров
            kobject_unlock(&container->lock);
            return ERR_BUSY;
        }
        // взяли последний номер из диапазона номеров, описанного узлом
        id = node->key + ((int) node->data) - 1;
        if (((int) node->data) == 1) {
            // узел, соответствующий одному номеру
            rb_tree_remove(&container->inverted_free_ids[container->current_free_subspace], node);
            kfree(node);
        } else {
            node->data = (void *) (((int) node->data) - 1);
        }
        id = RES_ID_INVERT(id, container->numlimit);
    } else {
        // захват заданного номера, если он свободен
        node = rb_tree_search(&container->objects, id);
        if (node != NULL) {
            kobject_unlock(&container->lock);
            return ERR_BUSY;
        }
    }
    memlen = RES_NODE_BASE_SIZE + len;
    container->memused += memlen;
    node = (struct rb_node *) kmalloc_aligned(memlen, 8);
    rb_node_init(node);
    node->key = id;
    inner_hdr = (res_node_header_t *) &node->data;
    kobject_lock_init_locked(&inner_hdr->lock);
    inner_hdr->user_header.ref = NULL;
    inner_hdr->user_header.type = 0;
    *((size_t *) &inner_hdr->user_header.datalen) = len;
    rb_tree_insert(&container->objects, node);
    *hdr = &inner_hdr->user_header;
    kobject_unlock_inherit(&container->lock, &inner_hdr->lock);
    return id;
}

int resm_search_and_lock (res_container_t *c, int id, struct res_header **hdr)
{
    inner_res_container_t *container = *c;
    res_node_header_t *inner_hdr;
    if ((container == NULL) || (hdr == NULL)) {
        return ERR_ILLEGAL_ARGS;
    }
    if(container->finalizing) {
        // рекурсивный вызов из res_free недопустим
        syshalt(SYSHALT_RESM_RECURSION_ERROR);
        return 0;
    }
    int res = ERR;

    while (1) {
        kobject_lock(&container->lock);
        struct rb_node *node = rb_tree_search(&container->objects, id);
        if (node == NULL) {
            kobject_unlock(&container->lock);
            return res;
        }
        inner_hdr = (res_node_header_t *) &node->data;
        res = spinlock_trylock(&inner_hdr->lock.slock);
        if (res == 1) {
            *hdr = &inner_hdr->user_header;
            kobject_unlock_inherit(&container->lock, &inner_hdr->lock);
            break;
        }
        kobject_unlock(&container->lock);
        cpu_wait_energy_save(); // притормозим текущее ядро до следующего события
        // TODO расчет в любом случае на короткий период времени блокировки объекта пространства другим ядром,
        // поэтому более длительные паузы или более сложные методы синхронизации не применяем сейчас
    }
    return OK;
}

int resm_unlock (res_container_t *c, struct res_header *hdr)
{
    inner_res_container_t *container = *c;
    if ((container == NULL) || (hdr == NULL)) {
        return ERR_ILLEGAL_ARGS;
    }
    if(container->finalizing) {
        // рекурсивный вызов из res_free недопустим
        syshalt(SYSHALT_RESM_RECURSION_ERROR);
        return 0;
    }
    res_node_header_t *inner_hdr = (void *) hdr - sizeof(kobject_lock_t);
    kobject_unlock(&inner_hdr->lock);
    return OK;
}

static int _res_remove (res_container_t *c, struct res_header *hdr, int locked)
{
    inner_res_container_t *container = *c;
    if ((container == NULL) || (hdr == NULL)) {
        return ERR_ILLEGAL_ARGS;
    }
    if(container->finalizing) {
        // рекурсивный вызов из res_free недопустим
        syshalt(SYSHALT_RESM_RECURSION_ERROR);
        return 0;
    }

    res_node_header_t *inner_hdr = (void *) hdr - sizeof(kobject_lock_t);
    struct rb_node *node = (void *) inner_hdr - sizeof(struct rb_node) + sizeof(void *);
    int inverted_id = RES_ID_INVERT(node->key, container->numlimit);
    int res;

    while (1) {
        kobject_lock(&container->lock);
        if (locked) {
            break;
        }
        res = spinlock_trylock(&inner_hdr->lock.slock);
        if (res == 1) {
            break;
        }
        kobject_unlock(&container->lock);
        cpu_wait_energy_save(); // притормозим текущее ядро до следующего события
        // TODO расчет в любом случае на короткий период времени блокировки объекта пространства другим ядром,
        // поэтому более длительные паузы или более сложные методы синхронизации не применяем сейчас
    }
    rb_tree_remove(&container->objects, node);
    int target_free_subspace = container->current_free_subspace ^ 1;
    if (container->gen_strategy != RES_ID_GEN_STRATEGY_NOGEN) {
        // необходимо вернуть номер в хранилище свободных номеров
        struct rb_node *fnode = rb_tree_search_neareqless(
                &container->inverted_free_ids[target_free_subspace], inverted_id);
        if ((fnode != NULL) && (((int) fnode->key) + ((int) fnode->data) == inverted_id)) {
            // выполняем слияние с диапазоном свободных номеров слева
            fnode->data = (void *) (((int) fnode->data) + 1);
        } else {
            fnode = rb_tree_search(&container->inverted_free_ids[target_free_subspace], inverted_id + 1);
            if (fnode == NULL) {
                // нет слияния слева и справа, добавляем узел в дерево свободных номеров
                fnode = kmalloc(sizeof(struct rb_node));
                rb_node_init(fnode);
                rb_node_set_key(fnode, inverted_id);
                rb_node_set_data(fnode, (void *) 1);
                rb_tree_insert(&container->inverted_free_ids[target_free_subspace], fnode);
            } else {
                // выполняем слияние с диапазоном свободных номеров справа
                rb_tree_remove(&container->inverted_free_ids[target_free_subspace], fnode);
                fnode->key--;
                rb_node_set_data(fnode, (void *) (((int) fnode->data) + 1));
                rb_tree_insert(&container->inverted_free_ids[target_free_subspace], fnode);
            }
        }
    }

    container->memused = container->memused - hdr->datalen - RES_NODE_BASE_SIZE;
    //  необходима корректная разблокировка в 2-х случаях
    if (!locked) {
        spinlock_unlock(&inner_hdr->lock.slock);
    }
    kobject_unlock(&container->lock);
    if (locked) {
        kobject_unlock(&inner_hdr->lock);
    }
    kfree(node);        // память целевого узла освободили последней, это можно выполнить только после разблокировок

    return OK;
}

int resm_remove (res_container_t *c, struct res_header *hdr)
{
    return _res_remove(c, hdr, 0);
}

int resm_remove_locked (res_container_t *c, struct res_header *hdr)
{
    return _res_remove(c, hdr, 1);
}

int resm_search_remove (res_container_t *c, int id)
{
    struct res_header *hdr;
    inner_res_container_t *container = *c;
    if(container->finalizing) {
        // рекурсивный вызов из res_free недопустим
        syshalt(SYSHALT_RESM_RECURSION_ERROR);
        return 0;
    }
    int res = resm_search_and_lock(c, id, &hdr);
    if (res != OK) {
        return res;
    }
    return resm_remove_locked(c, hdr);
}

static void res_node_free (struct rb_node *node)
{
    kfree(node);
}

static void free_container (res_container_t *c, struct rb_node *node,
        void (*res_free) (res_container_t *c, int id, struct res_header *hdr))
{
    if (!node)
        return;
    struct rb_node *next_node = NULL;
    res_node_header_t *inner_hdr = (res_node_header_t *) &node->data;
    if (rb_tree_get_next(node, &next_node)) {
        free_container(c, next_node, res_free);
        next_node = NULL;
    }
    if (res_free != NULL) {
        res_free(c, node->key, &inner_hdr->user_header);
    }
    kfree(node);
    free_container(c, next_node, res_free);
}

int resm_container_free (res_container_t *c, void (*res_free) (res_container_t *c, int id, struct res_header *hdr))
{
    int cnt;
    inner_res_container_t *container = *c;
    if(container->finalizing) {
        // рекурсивный вызов из res_free недопустим
        syshalt(SYSHALT_RESM_RECURSION_ERROR);
        return 0;
    }
    container->finalizing = 1;
    kobject_lock(&container->lock);
    rb_tree_free(&container->inverted_free_ids[0], res_node_free);
    rb_tree_free(&container->inverted_free_ids[1], res_node_free);
    cnt = rb_tree_get_nodes_count(&container->objects);
    free_container(c, rb_tree_get_min(&container->objects), res_free);
    kobject_unlock(&container->lock);
    kfree(container);
    *c = NULL;
    return cnt;
}
