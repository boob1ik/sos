/* Информация хранится в двух красно-черных деревьях:

 - дерево всех блоков страниц (свободных и занятых)
 - ключ: адрес стартовой страницы блока

 - дерево свободных блоков страниц
 - ключ: размер блока (в страницах)
 - если блоков одного размера несколько, они хранятся в узле двусвязным списком
 */

#include "page.h"
#include <rbtree.h>
#include "kmem.h"
#include "dlist.h"
#include "common\syshalt.h"
#include <arch.h>
#include <syn/ksyn.h>

static kobject_lock_t pa_lock;

static inline void page_allocator_lock ()
{
    kobject_lock(&pa_lock);
}

static inline void page_allocator_unlock ()
{
    kobject_unlock(&pa_lock);
}

struct page_stat {
    size_t total;
    size_t free;
};

static struct page_allocator {
    size_t page_size;
    struct rb_tree *all;
    struct rb_tree *free;
    struct {
        struct page_stat all;
        struct page_stat fixed;
        struct page_stat dynamic;
    } stat;
} pa = { 0 };

size_t get_page_total ()
{
    return pa.stat.all.total;
}

size_t get_page_free ()
{
    return pa.stat.all.free;
}

size_t get_page_used ()
{
    return pa.stat.all.total - pa.stat.all.free;
}

size_t get_page_fixed_total ()
{
    return pa.stat.fixed.total;
}

size_t get_page_fixed_free ()
{
    return pa.stat.fixed.free;
}

size_t get_page_fixed_used ()
{
    return pa.stat.fixed.total - pa.stat.fixed.free;
}

size_t get_page_dynamic_total ()
{
    return pa.stat.dynamic.total;
}

size_t get_page_dynamic_free ()
{
    return pa.stat.dynamic.free;
}

size_t get_page_dynamic_used ()
{
    return pa.stat.dynamic.total - pa.stat.dynamic.free;
}

// данные узла дерева свободных блоков
struct fdata {
    bool multi;
    union {
        struct rb_node *anode;           // single
        struct dlist *anode_dlist_head;  // multi
    };
};

// данные узла дерева всех блоков
struct adata {
    bool used;
    bool fixed;
    int allocate_cnt;
    uint32_t size;
    struct rb_node *fnode;
};

struct fnode {
    struct rb_node rb_node;
    struct fdata data;
};

struct anode {
    struct rb_node rb_node;
    struct adata data;
};

static struct rb_node* alloc_fnode ()
{
    struct rb_node *node = NULL;
    size_t size_node = sizeof(*node) - sizeof(node->data)
            + sizeof(struct fdata);
    node = kmalloc(size_node);
    memset(node, 0, size_node);
    rb_node_init(node);
    return node;
}

static struct rb_node* alloc_anode ()
{
    struct rb_node *node = NULL;
    size_t size_node = sizeof(*node) - sizeof(node->data)
            + sizeof(struct adata);
    node = kmalloc(size_node);
    memset(node, 0, size_node);
    rb_node_init(node);
    return node;
}

static inline void insert_node (struct rb_tree *tree, struct rb_node *node,
        uint32_t key)
{
    rb_node_set_key(node, key);
    rb_tree_insert(tree, node);
}

static bool is_anode_fixed (struct rb_node *anode)
{
    struct adata *adata = (struct adata *) &anode->data;
    return adata->fixed ? true : false;
}

static bool is_anode_used (struct rb_node *anode)
{
    struct adata *adata = (struct adata *) &anode->data;
    return adata->used;
}

static uint32_t get_anode_size (struct rb_node *anode)
{
    struct adata *adata = (struct adata*) &anode->data;
    return adata->size;
}

static void set_anode_fixed (struct rb_node *anode)
{
    struct adata *adata = (struct adata *) &anode->data;
    adata->fixed = true;
}

static void set_anode_free (struct rb_node *anode)
{
    struct adata *adata = (struct adata *) &anode->data;
    adata->used = false;
}

static void set_anode_used (struct rb_node *node)
{
    struct adata *adata = (struct adata *) &node->data;
    adata->used = true;
}

static void link_anode_to_fnode (struct rb_node *anode, struct rb_node *fnode)
{
    struct adata *adata = (struct adata *) &anode->data;
    adata->fnode = fnode;
}

static void set_anode_size (struct rb_node *node, uint32_t new_size)
{
    struct adata *adata = (struct adata *) &node->data;
    adata->size = new_size;
}

static struct rb_node* get_anode (struct rb_node *fnode)
{
    struct fdata *data = (struct fdata *) &fnode->data;
    if (data->multi) {
        return dlist_get_entry(data->anode_dlist_head);
    } else {
        return data->anode;
    }
}

static struct rb_node* get_fnode (struct rb_node *anode)
{
    if (is_anode_fixed(anode))
        return NULL;
    struct adata *adata = (struct adata *) &anode->data;
    if (adata->used)
        return NULL;
    return adata->fnode;
}

static void* get_fnode_adr (struct rb_node *fnode)
{
    return (void*) rb_node_get_key(get_anode(fnode));
}

static void* get_anode_adr (struct rb_node *anode)
{
    return (void*) rb_node_get_key(anode);
}

static void insert_fnode (uint32_t size, struct rb_node *anode)
{
    struct rb_node *fnode = rb_tree_search(pa.free, size);
    if (!fnode) {
        fnode = alloc_fnode();
        struct fdata *data = (struct fdata *) &fnode->data;
        data->multi = false;
        data->anode = anode;
        insert_node(pa.free, fnode, size);
    } else {
        struct fdata *data = (struct fdata *) &fnode->data;
        if (data->multi) {
            struct dlist *dlist = kmalloc(sizeof(*dlist));
            dlist_init(dlist);
            dlist->entry = (void*) anode;
            dlist_insert(data->anode_dlist_head, dlist);
        } else {
            struct dlist *dlist = kmalloc(sizeof(*dlist));
            dlist_init(dlist);
            dlist->entry = data->anode;
            struct dlist *dlist_new = kmalloc(sizeof(*dlist_new));
            dlist_init(dlist_new);
            dlist_new->entry = anode;
            dlist_insert(dlist, dlist_new);
            data->anode_dlist_head = dlist;
            data->multi = true;
        }
    }
    link_anode_to_fnode(anode, fnode);
    set_anode_free(anode);

    pa.stat.dynamic.free += size;
}

static void remove_fnode (struct rb_node *fnode)
{
    struct fdata *fdata = (struct fdata *) &fnode->data;
    if (fdata->multi) {
        struct dlist *dlist = fdata->anode_dlist_head;
        fdata->anode_dlist_head = fdata->anode_dlist_head->next;
        dlist_remove(dlist);
        kfree(dlist);
        dlist = fdata->anode_dlist_head;

        if (!dlist->next) {
            fdata->anode = dlist_get_entry(dlist);
            fdata->multi = false;
            kfree(dlist);
        }

        return;
    }

    rb_tree_remove(pa.free, fnode);
    pa.stat.dynamic.free -= rb_node_get_key(fnode);
    kfree(fnode);
}

static struct rb_node* insert_anode (void *adr, uint32_t size)
{
    struct rb_node *anode = alloc_anode();
    set_anode_used(anode);
    set_anode_size(anode, size);
    insert_node(pa.all, anode, (uint32_t) adr);
    return anode;
}

static void remove_anode (struct rb_node *node)
{
    rb_tree_remove(pa.all, node);
    kfree(node);
}

static struct rb_node* clone_anode (struct rb_node *anode)
{
    struct rb_node *clone = alloc_anode();
    memcpy(clone, anode, sizeof(*clone));
    return clone;
}

static bool is_contiguous (struct rb_node *anode1, struct rb_node *anode2)
{
    if (get_anode_adr(anode1) + get_anode_size(anode1) * mmu_page_size() == get_anode_adr(anode2))
        return true;

    if (get_anode_adr(anode2) + get_anode_size(anode2) * mmu_page_size() == get_anode_adr(anode1))
        return true;

    return false;
}

static bool merge_anode (struct rb_node *anode_master,
        struct rb_node *anode_slave)
{
    if (!anode_master || !anode_slave)
        return false;

    if (is_anode_used(anode_master) != is_anode_used(anode_slave))
        return false;

    if (!is_contiguous(anode_master, anode_slave))
        return false;

    if ((is_anode_used(anode_master) && is_anode_used(anode_slave))
            || (is_anode_fixed(anode_master) && is_anode_fixed(anode_slave))) {
        set_anode_size(anode_master,
                get_anode_size(anode_master) + get_anode_size(anode_slave));
        remove_anode(anode_slave);
    } else {
        if (is_anode_fixed(anode_master) || is_anode_fixed(anode_slave))
            return false;

        uint32_t size = get_anode_size(anode_master)
                + get_anode_size(anode_slave);

        remove_fnode(get_fnode(anode_slave));
        remove_anode(anode_slave);

        remove_fnode(get_fnode(anode_master));
        set_anode_size(anode_master, size);
        insert_fnode(size, anode_master);
    }
    return true;
}

static struct rb_node* split_anode (struct rb_node *anode, size_t new_size)
{
    if (new_size == 0)
        return NULL;

    size_t size = get_anode_size(anode);
    if (size <= new_size)
        return NULL;

    struct rb_node *cutoff = clone_anode(anode);
    set_anode_size(anode, new_size);
    set_anode_size(cutoff, size - new_size);

    return cutoff;
}

static void free_anode (struct rb_node *anode)
{
    if (!anode)
        syshalt(SYSHALT_OOPS_ERROR);

    if (!is_anode_used(anode))
        return;

    if (is_anode_fixed(anode)) {
        set_anode_free(anode);
    } else
        insert_fnode(get_anode_size(anode), anode);

    struct rb_node *less = rb_tree_get_nearless(anode);
    if (less) {
        if (merge_anode(less, anode))
            anode = less;
    }

    struct rb_node *larger = rb_tree_get_nearlarger(anode);
    if (larger) {
        merge_anode(anode, larger);
    }
}

static struct rb_node* alloc_from_fnode (struct rb_node *fnode, uint32_t size)
{
    if (!fnode || !size)
        return NULL;

    uint32_t size_node = fnode->key;
    void *adr = get_fnode_adr(fnode);
    struct rb_node *anode = get_anode(fnode);

    set_anode_used(anode);
    set_anode_size(anode, size);
    remove_fnode(fnode);

    if (size_node > size) {
        // остаток вставим новым пустым блоком
        struct rb_node *cutoff = insert_anode(adr + pa.page_size * size,
                size_node - size);
        free_anode(cutoff);
        return cutoff;
    }

    return NULL;
}

void page_allocator_init (size_t page_size)
{
    kobject_lock_init(&pa_lock);

    pa.page_size = page_size;
    pa.all = NULL;
    pa.free = NULL;
    pa.stat.all.total = 0;
    pa.stat.all.free = 0;
    pa.stat.fixed.total = 0;
    pa.stat.fixed.free = 0;
    pa.stat.dynamic.total = 0;
    pa.stat.dynamic.free = 0;

    pa.all = kmalloc(sizeof(*pa.all));
    rb_tree_init(pa.all);
    rb_tree_set_mode(pa.all, RBTREE_BY_KEY_VALUE);

    pa.free = kmalloc(sizeof(*pa.free));
    rb_tree_init(pa.free);
    rb_tree_set_mode(pa.free, RBTREE_BY_KEY_VALUE);
}

int page_allocator_add (void *start, uint32_t n, bool fixed)
{
    pa.stat.all.total += n;
    if (fixed)
        pa.stat.fixed.total += n;
    else
        pa.stat.dynamic.total += n;

    struct rb_node *anode = insert_anode(start, n);
    if (fixed) {
        set_anode_fixed(anode);
    }
    free_anode(anode);
    return OK;
}

void* page_alloc_fixed (size_t adr, uint32_t n, bool multiple_access)
{
    if (!n)
        return NULL;

    page_allocator_lock();

    struct rb_node *anode = rb_tree_search_neareqless(pa.all, adr);
    if (!anode) {
        page_allocator_unlock();
        return NULL;
    }

    if (is_anode_used(anode)) {
        page_allocator_unlock();
        return NULL;
    }

    size_t node_adr = rb_node_get_key(anode);
    size_t node_size = get_anode_size(anode);
    size_t shift_to_start = (adr - node_adr) / mmu_page_size();

    if (node_size - shift_to_start < n)
        return NULL;

    if (is_anode_fixed(anode)) {
        struct rb_node *cutoff = NULL;
        if (shift_to_start) {
            cutoff = split_anode(anode, shift_to_start);
            insert_node(pa.all, cutoff, adr);
        } else
            cutoff = anode;

        anode = cutoff;
        cutoff = split_anode(anode, n);
        if (cutoff) {
            insert_node(pa.all, cutoff, adr + n * mmu_page_size());
        }

        set_anode_used(anode);
    } else {
        struct rb_node *fnode = get_fnode(anode);

        if (adr == node_adr && n == node_size) {
            set_anode_used(get_anode(fnode));
            remove_fnode(fnode);
        } else if (adr == node_adr) {
            alloc_from_fnode(fnode, n);
        } else {
            set_anode_used(anode);
            set_anode_size(anode, node_size);
            remove_fnode(fnode);

            struct rb_node *alloc_node = split_anode(anode,
                    (adr - node_adr) / mmu_page_size());

            insert_node(pa.all, alloc_node, adr);
            free_anode(alloc_node);

            struct rb_node *cutoff = alloc_from_fnode(get_fnode(alloc_node), n);
            if (cutoff) {
                merge_anode(cutoff, rb_tree_get_nearlarger(cutoff));
            }

            free_anode(anode);
        }
    }

    page_allocator_unlock();
    return (void*) adr;
}

void* page_alloc (uint32_t n)
{
    if (!n)
        return NULL;

    page_allocator_lock();

    struct rb_node *fnode = rb_tree_search_eqlarger(pa.free, n);
    if (!fnode) {
        page_allocator_unlock();
        return NULL;
    }

    void *adr = get_fnode_adr(fnode);
    alloc_from_fnode(fnode, n);
    page_allocator_unlock();
    return adr;
}

// realtime = 1 - поиск блока двойного размера и возврат ошибки если такого нет
// realtime = 0 - попытка разместить выровненный блок в любом блоке большего размера - если такого нет - PANIC
void* page_alloc_align (uint32_t n /* в страницах */,
        uint32_t align /* в байтах */, uint32_t realtime)
{
    if (!n)
        return NULL;

    page_allocator_lock();

    struct rb_node *fnode = rb_tree_search_neareqlarger(pa.free, n);
    while (fnode) {
        void *adr = NULL;

        void *node_adr = get_fnode_adr(fnode);
        size_t node_size = rb_node_get_key(fnode);
        struct rb_node *anode = get_anode(fnode);

        adr = (void*) (((size_t) node_adr / align) * align + align);

        if (adr + n * mmu_page_size() <= node_adr + node_size * mmu_page_size()) {
            if (adr == node_adr && n == node_size) {
                set_anode_used(get_anode(fnode));
                remove_fnode(fnode);
            } else if (adr == node_adr) {
                alloc_from_fnode(fnode, n);
            } else {
                set_anode_used(anode);
                set_anode_size(anode, node_size);
                remove_fnode(fnode);

                struct rb_node *alloc_node = split_anode(anode,
                        (adr - node_adr) / mmu_page_size());

                insert_node(pa.all, alloc_node, (uint32_t) adr);
                free_anode(alloc_node);

                struct rb_node *cutoff = alloc_from_fnode(get_fnode(alloc_node),
                        n);
                if (cutoff) {
                    merge_anode(cutoff, rb_tree_get_nearlarger(cutoff));
                }

                free_anode(anode);
            }

            page_allocator_unlock();
            return adr;
        }

        fnode = rb_tree_get_nearlarger(fnode);
    }

    page_allocator_unlock();
    return NULL;
}

void page_free (void *adr)
{
    if (!adr)
        return;

    page_allocator_lock();
    free_anode(rb_tree_search(pa.all, (size_t) adr));
    page_allocator_unlock();
}
