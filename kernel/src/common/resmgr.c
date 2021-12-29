#include "resmgr.h"
#include <mem/kmem.h>
#include <string.h>
#include <limits.h>
#include <config.h>

/**
 * Хранилище объектов по ID номерам с синхронизированными методами доступа.
 * Объекты помещаются копированием в динамическую память хранилища.
 * Из хранилища объекты доступны по ссылке на копию данных.
 * Для экономии памяти размер данных копии в явном виде не хранится.
 * Размер объектов может быть произвольно разным, идентификация типов объектов и размера является
 * задачей более высокого уровня и обычно сводится к наличию поля типа и поля размера где нужно.
 *
 * Основная идея реализации нашего универсального контейнера - максимальная производительность вместе с
 * универсальностью. Для этого предусматриваем объединение выделяемой памяти на каждый узел
 * к-ч. дерева хранилища с копией данных объекта.
 */

static int res_container_init_local (struct res_container *container)
{
    int res = OK;

    kobject_lock_init(&container->lock);
    rb_tree_init(&container->objects);
    rb_tree_init(&container->free_dynids);

    if ((container->id_cnt > 0)
            && ((container->id < RES_ID_MIN)
                    || (container->id + container->id_cnt - 1 > INT_MAX))) {
        // некорректный статический диапазон, чистим его
        container->id = 0;
        container->id_cnt = 0;
        res = ERR;
    }
    if ((container->dynid_cnt > 0)
            && ((container->dynid < RES_ID_MIN)
                    || (container->dynid + container->dynid_cnt - 1 > INT_MAX))) {
        // некорректный динамический диапазон, чистим его
        container->dynid = 0;
        container->dynid_cnt = 0;
        res = ERR;
    }
    // проверка на перекрытие, диапазоны не должны перекрываться для нашего алгоритма хранилища
    if (((container->dynid >= container->id)
            && (container->dynid < container->id + container->id_cnt))
            || ((container->id >= container->dynid)
                    && (container->id < container->dynid + container->dynid_cnt))) {
        container->id = 0;
        container->id_cnt = 0;
        container->dynid = 0;
        container->dynid_cnt = 0;
        res = ERR;
    }

    struct rb_node *node = (struct rb_node *) kmalloc(sizeof(struct rb_node));
    rb_node_init(node);
    rb_node_set_key(node, container->dynid);
    rb_node_set_data(node, (void *) (container->dynid_cnt));
    rb_tree_insert(&container->free_dynids, node);
    return res;
}

int res_container_init(struct res_container *container, int min, int max, int min_dynamic, int max_dynamic) {
    container->id = min;
    container->id_cnt = max - min + 1;
    container->dynid = min_dynamic;
    container->dynid_cnt = max_dynamic - min_dynamic + 1;
    return res_container_init_local(container);
}

int res_container_init_static(struct res_container *container, int min, int max) {
    container->id = min;
    container->id_cnt = max - min + 1;
    container->dynid = 0;
    container->dynid_cnt = 0;
    return res_container_init_local(container);
}

int res_container_init_dynamic(struct res_container *container, int min_dynamic, int max_dynamic) {
    container->id = 0;
    container->id_cnt = 0;
    container->dynid = min_dynamic;
    container->dynid_cnt = max_dynamic - min_dynamic + 1;
    return res_container_init_local(container);
}

int res_container_init_default(struct res_container *container) {
    container->id = MIN_RES_STATIC_ID;
    container->id_cnt = MAX_RES_STATIC_ID - MIN_RES_STATIC_ID + 1;
    container->dynid = MIN_RES_DYNAMIC_ID;
    container->dynid_cnt = MAX_RES_DYNAMIC_ID - MIN_RES_DYNAMIC_ID + 1;
    return res_container_init_local(container);
}

static void res_node_free(struct rb_node *node) {
    kfree(node);
}

static void free_by_nfo(struct rb_node *node, void (*res_free) (int id, void *res)) {
    if (!node)
        return;
    struct rb_node *next_node = NULL;
    if (rb_tree_get_next(node, &next_node)) {
        free_by_nfo(next_node, res_free);
        next_node = NULL;
    }
    if(res_free != NULL) {
        res_free(node->key, &node->data);
    }
    kfree(node);
    free_by_nfo(next_node, res_free);
}

static void free_by_ref(struct rb_node *node, void (*ref_free) (int id, void *obj)) {
    if (!node)
        return;
    struct rb_node *next_node = NULL;
    if (rb_tree_get_next(node, &next_node)) {
        free_by_ref(next_node, ref_free);
        next_node = NULL;
    }
    if(ref_free != NULL) {
        ref_free(node->key, node->data);
    }
    kfree(node);
    free_by_ref(next_node, ref_free);
}

int res_container_free(struct res_container *container, void (*res_free) (int id, void *res)) {
    int cnt;
    kobject_lock(&container->lock);
    rb_tree_free(&container->free_dynids, res_node_free);
    cnt = rb_tree_get_nodes_count(&container->objects);
    free_by_nfo(rb_tree_get_min(&container->objects), res_free);
    kobject_unlock(&container->lock);
    return cnt;
}

int res_container_byref_free(struct res_container *container, void (*ref_free) (int id, void *obj)) {
    int cnt;
    kobject_lock(&container->lock);
    rb_tree_free(&container->free_dynids, res_node_free);
    cnt = rb_tree_get_nodes_count(&container->objects);
    free_by_ref(rb_tree_get_min(&container->objects), ref_free);
    kobject_unlock(&container->lock);
    return cnt;
}

int res_put_ref(struct res_container *container, int id, void *obj) {
    struct res_info nfo = {obj, 0};
    return res_put(container, id, &nfo);
}

int res_put (struct res_container *container, int id, struct res_info *nfo)
{
    int res = ERR;
    struct rb_node *node;
    if(nfo == NULL) return res;
    // не допускаем попадания не нулевого положительного номера за пределы
    // статического диапазона
    if (!(((id >= container->id) && (id <= container->id - 1 + container->id_cnt)) || (id == 0))) {
        return res;
    }
    kobject_lock(&container->lock);
    if (id < RES_ID_MIN) {
        // динамический захват любого свободного номера,
        // выполняем отсечением корневого диапазона справа,
        // так как не нужно удалять узел из памяти и выполнять
        // соответствующие дополнительные удаления/вставки
        node = rb_tree_search_eqlarger(&container->free_dynids, 0);
        if (node == NULL) {
            // контейнер переполнен, нет свободных номеров
            kobject_unlock(&container->lock);
            return ERR_BUSY;
        }
        res = node->key + ((int) node->data) - 1;
        if (((int) node->data) == 1) {
            // узел, соответствующий одному номеру
            rb_tree_remove(&container->free_dynids, node);
            kfree(node);
            node = (struct rb_node *) kmalloc_aligned(
                    sizeof(struct rb_node) + nfo->res_len, 8);
            rb_node_init(node);
            node->key = res;
            if (nfo->res_len > 0) {
                memcpy(&node->data, nfo->res, nfo->res_len);
            } else {
                node->data = nfo->res;
            }
            nfo->res = &node->data;
            rb_tree_insert(&container->objects, node);
        } else {
            // отсекаем из свободного и добавляем новый узел объекта в дерево объектов
            node->data = (void *) (((int) node->data) - 1);
            node = (struct rb_node *) kmalloc_aligned(
                    sizeof(struct rb_node) + nfo->res_len, 8);
            rb_node_init(node);
            node->key = res;
            if (nfo->res_len > 0) {
                memcpy(&node->data, nfo->res, nfo->res_len);
            } else {
                node->data = nfo->res;
            }
            nfo->res = &node->data;
            rb_tree_insert(&container->objects, node);
        }
    } else {
        // захват заданного номера, если он свободен
        node = rb_tree_search(&container->objects, id);
        if (node != NULL) {
            res = ERR_BUSY;
        } else {
            // а вот здесь в силу свойства неперекрывающихся диапазонов
            // нам не нужно работать с деревом свободных номеров,
            // только создаем новый узел ресурса и добавляем его в дерево объектов
            res = id;
            node = kmalloc_aligned(
                    sizeof(struct rb_node) + nfo->res_len, 8);
            rb_node_init(node);
            node->key = res;
            if (nfo->res_len > 0) {
                memcpy(&node->data, nfo->res, nfo->res_len);
            } else {
                node->data = nfo->res;
            }
            nfo->res = &node->data;
            rb_tree_insert(&container->objects, node);
        }
    }
    kobject_unlock(&container->lock);
    return res;
}

void *res_get_ref(struct res_container *container, int id) {
    struct res_info nfo = {NULL, 0};
    res_get(container, id, &nfo);
    if(nfo.res == NULL) {
        return NULL;
    }
    return (void *)(*((size_t *)nfo.res));
}

int res_get (struct res_container *container, int id, struct res_info *nfo)
{
    int res = ERR;
    if(nfo == NULL) return res;
    kobject_lock(&container->lock);
    struct rb_node *node = rb_tree_search(&container->objects, id);
    if (node != NULL) {
        nfo->res = &node->data;
        nfo->res_len = 0;
        res = OK;
    }
    kobject_unlock(&container->lock);
    return res;
}

int res_remove (struct res_container *container, int id)
{
    int res = ERR;
    kobject_lock(&container->lock);
    struct rb_node *node = rb_tree_search(&container->objects, id);
    if (node != NULL) {
        rb_tree_remove(&container->objects, node);
        res = OK;
        if( (id >= container->dynid) && (id < container->dynid + container->dynid_cnt) ) {
            struct rb_node *fnode = rb_tree_search_neareqless(&container->free_dynids,
                    id);
            if ((fnode != NULL)
                    && (((int) fnode->key) + ((int) fnode->data) == id)) {
                kfree(node);
                // выполняем слияние с диапазоном свободных номеров слева
                fnode->data = (void *) (((int) fnode->data) + 1);
            } else {
                fnode = rb_tree_search(&container->free_dynids, id + 1);
                if (fnode == NULL) {
                    // нет слияния слева и справа, переводим узел в дерево свободных номеров
                    rb_node_set_data(node, (void *) 1);
                    rb_tree_insert(&container->free_dynids, node);
                } else {
                    kfree(node);
                    // выполняем слияние с диапазоном свободных номеров справа
                    rb_tree_remove(&container->free_dynids, fnode);
                    fnode->key--;
                    rb_node_set_data(node, (void *) (((int) fnode->data) + 1));
                    rb_tree_insert(&container->free_dynids, fnode);
                }
            }
        } else {
            kfree(node);
        }
    }
    kobject_unlock(&container->lock);
    return res;
}

