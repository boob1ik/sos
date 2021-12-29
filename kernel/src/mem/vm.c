#include "vm.h"
#include "mmutbl_pool.h"
#include <arch.h>
#include <string.h>
#include "page.h"
#include "kmem.h"
#include <common/utils.h>
#include <common/printf.h>
#include "../sched.h"
#include <syn/ksyn.h>
#include "common/log.h"
#include "proc.h"
#include "common\error.h"

#define DEFAULT_STACK_SIZE    mmu_page_size()

static inline uint32_t get_dir (uint32_t va) { return va / mmu_dir_size(); }

static struct {
    struct mmap *map;
    int pool;
} cur_map[NUM_CORE];

static struct stat {
    int maps;
    int segs;
    int page_tables;
} stat = {0};

static kobject_lock_t mmulock;

static inline void mmu_lock ()
{
    kobject_lock(&mmulock);
}

static inline void mmu_unlock ()
{
    kobject_unlock(&mmulock);
}

static uint32_t flush_tlb_flags[NUM_CORE] = { 0 };

static void flush_tlb_core (uint32_t core)
{
    //ipi_send(core, IPI_FLUSH_TLB);
    flush_tlb_flags[cpu_get_core_id()] |= 1 << core;
}

static void flush_tlb_smp (struct mmap *map)
{
    for (int i = 0; i < NUM_CORE; i++) {
        if (cpu_get_core_id() == i)
            continue;
        if (cur_map[i].map == map)
            flush_tlb_core(i);
    }
    while (flush_tlb_flags[cpu_get_core_id()])
        ;
}

static void map_page (uint32_t *entry, uint32_t va, mem_attributes_t attr)
{
    uint8_t shift = (va % mmu_dir_size()) / mmu_page_size();
    mmu_lock();
    mmu_set_pgte(entry + shift, va, attr);
    flush_tlb();
    mmu_unlock();
}

static void unmap_page (uint32_t *entry)
{
    mmu_lock();
    mmu_set_fault(entry);
    flush_tlb();
    mmu_unlock();
}

static void map_pagetable (uint32_t va, uint32_t *pgt)
{
    mmu_lock();
    mmu_set_pgt(mmu_get_tbl() + get_dir(va), pgt);
    flush_tlb();
    mmu_unlock();
}

static void map_dir (uint32_t va, mem_attributes_t attr)
{
    mmu_lock();
    mmu_set_dir(mmu_get_tbl() + get_dir(va), va, attr);
    flush_tlb();
    mmu_unlock();
}

static void unmap_dir (uint32_t va)
{
    mmu_lock();
    mmu_set_fault(mmu_get_tbl() + get_dir(va));
    flush_tlb();
    mmu_unlock();
}

static void seg_free (struct seg *seg)
{
    switch (seg->type) {
    case SEG_NORMAL:
        page_free(seg->adr);
        break;
    case SEG_STACK:
        page_free(seg->adr - mmu_page_size());
        break;
    }
}

static bool seg_dir (const struct seg *seg)
{
    return (!((uint32_t) seg->adr % mmu_dir_size()) && !(seg->size % mmu_dir_size())) ? true : false;
}

static void* new_pagetable ()
{
    ++stat.page_tables;
    void *pgt = kmalloc_aligned(mmu_pagetable_size(), mmu_pagetable_size());
    bzero(pgt, mmu_pagetable_size());
    return pgt;
}

static void delete_pagetable (void *p)
{
    --stat.page_tables;
    kfree(p);
}

struct seg* seg_create (struct mmap* map, void *adr, size_t size, mem_attributes_t attr)
{
    struct seg *seg = kmalloc(sizeof(*seg));
    seg->adr = adr;
    seg->size = size;
    seg->attr = attr;
    seg->ref = 0;
    seg->shared = NULL;
    seg->type = SEG_NORMAL;
    seg->map = map;
    ++stat.segs;
    return seg;
}

static int seg_insert (struct mmap *map, struct seg *seg)
{
    struct rb_node *node = kmalloc(sizeof(*node));
    rb_node_init(node);
    rb_node_set_data(node, (void*) seg);
    rb_node_set_key(node, (size_t) seg->adr);
    rb_tree_insert(map->segs, node);
    map->size += seg->size;
    seg->ref++;
    return OK;
}

static int seg_remove (struct mmap *map, struct seg *seg)
{
    struct rb_node *node = rb_tree_search(map->segs, (size_t) seg->adr);
    if (!node)
        return ERROR(ERR);
    map->size -= seg->size;
    rb_tree_remove(map->segs, node);
    kfree(node);
    seg->ref--;
    return OK;
}

static inline void map_lock (struct mmap *map)
{
    if (map == kmap)
        return;

    kobject_lock(&map->lock);
}

static inline void map_unlock (struct mmap *map)
{
    if (map == kmap)
        return;

    kobject_unlock(&map->lock);
    flush_tlb_smp(map);
}

static struct pgd* get_pgd (struct mmap *map, uint32_t dir)
{
    struct rb_node *node = rb_tree_search(map->pgds, dir);
    return node ? (struct pgd*)&node->data : NULL;
}

static struct pgd* alloc_pgd (struct mmap *map, uint32_t dir)
{
    struct pgd *pgd = get_pgd(map, dir);
    if (pgd)
        return pgd;

    struct rb_node *node = kmalloc(sizeof(*node) - sizeof(*node->data) + sizeof(struct pgd));
    bzero(node, sizeof(*node) - sizeof(*node->data) + sizeof(struct pgd));
    rb_node_init(node);
    rb_node_set_key(node, dir);
    rb_tree_insert(map->pgds, node);
    return (struct pgd*)(&node->data);
}

static void free_pgd (struct rb_tree *tree, struct pgd *pgd, uint32_t dir)
{
    struct rb_node *node = rb_tree_search(tree, dir);
    rb_tree_remove(tree, node);
    delete_pagetable(pgd->pgt);
    kfree(node);
}

static inline bool active_map (struct mmap *map)
{
    return map == cur_map[cpu_get_core_id()].map;
}

static void init_pgt_from_kmap (struct pgd *pgd, struct pgd *kpgd)
{
    if (mmu_is_dir(&kpgd->entry)) {
        for (int i = 0; i < mmu_page_in_dir(); i++) {
            struct seg *seg = vm_seg_get(kmap, (void*)mmu_get_base_addr_dir(&kpgd->entry));
            mmu_set_pgte(pgd->pgt + i, mmu_get_base_addr_dir(&kpgd->entry) + i * mmu_page_size(), seg->attr);
        }
    } else {
        memcpy(pgd->pgt, kpgd->pgt, mmu_pagetable_size());
    }
    pgd->kpgd = kpgd;
}

static void map_kdir (uint32_t va, mem_attributes_t attr)
{
    mmu_lock();
    for (int i = 0; i < get_mmutbl_pool_size(); i++) {
        mmu_set_dir(get_mmutbl_pool(i) + get_dir(va), va, attr);
    }
    flush_tlb();
    mmu_unlock();
}

static void map_kpagetable (uint32_t va, uint32_t *pgt)
{
    mmu_lock();
    for (int i = 0; i < get_mmutbl_pool_size(); i++) {
        mmu_set_pgt(get_mmutbl_pool(i) + get_dir(va), pgt);
    }
    flush_tlb();
    mmu_unlock();
}

static void map_kpage (uint32_t va, mem_attributes_t attr)
{
    uint8_t shift = (va % mmu_dir_size()) / mmu_page_size();
    mmu_lock();
    for (int i = 0; i < get_mmutbl_pool_size(); i++) {
        mmu_set_pgte(((uint32_t *)mmu_get_base_addr_dir(get_mmutbl_pool(i) + get_dir(va))) + shift, va, attr);
    }
    flush_tlb();
    mmu_unlock();
}

void mmap (struct mmap *map, const struct seg *seg)
{
    uint32_t va = (uint32_t) seg->adr;
    size_t pages = seg->size / mmu_page_size();
    mem_attributes_t attr = seg->attr;

    map_lock(map);
    while (pages > 0) {
        struct pgd *pgd = alloc_pgd(map, get_dir(va));

        if (seg_dir(seg)) {
            if (map == kproc.mmap || !rb_tree_search(kmap->pgds, get_dir(va))) {
                mmu_set_dir(&pgd->entry, va, attr);
                if (map == kproc.mmap) {
                    map_kdir(va, attr);
                } else if (active_map(map)) {
                    map_dir(va, attr);
                }
                pgd->cnt = mmu_page_in_dir();
                va += mmu_dir_size();
                pages -= mmu_page_in_dir();
                continue;
            }
        }

        if (!pgd->pgt) {
            pgd->pgt = new_pagetable();
            pgd->cnt = 0;
            if (map != kproc.mmap) {
                struct rb_node *kmap_node = rb_tree_search(kmap->pgds, get_dir(va));
                if (kmap_node) {
                    init_pgt_from_kmap(pgd, (struct pgd*)(&kmap_node->data));
                }
            }

            mmu_set_pgt(&pgd->entry, pgd->pgt);
            if (map == kproc.mmap) {
                map_kpagetable(va, pgd->pgt);
            } else if (active_map(map)) {
                map_pagetable(va, pgd->pgt);
            }
        }

        do {
            if (map == kproc.mmap)
                map_kpage(va, attr);
            else
                map_page(pgd->pgt, va, attr);

            ++pgd->cnt;
            va += mmu_page_size();
        } while (--pages && ((va % mmu_dir_size()) != 0));
    }
    seg_insert(map, (struct seg*)seg);

    if (map != cur_map[cpu_get_core_id()].map && map->mmu_pool != MAP_NO_POOL) {
        release_mmutbl_pool(map->mmu_pool);
        map->mmu_pool = MAP_NO_POOL;
    }

    map_unlock(map);
}

static void unmap_kpage (uint32_t *entry, struct pgd *pgd, uint32_t va)
{
    if (pgd->kpgd->pgt) {
        uint8_t shift = (va % mmu_dir_size()) / mmu_page_size();
        memcpy(entry, pgd->kpgd->pgt + shift, sizeof(*entry));
    } else {
        struct seg *seg = vm_seg_get(kmap, (void*)mmu_get_base_addr_dir(&pgd->kpgd->entry));
        mmu_set_pgte(entry, va, seg->attr);
    }
}

static void unmap (struct mmap *map, struct seg *seg)
{
    uint32_t va = (uint32_t) seg->adr;
    size_t pages = seg->size / mmu_page_size();

    map_lock(map);
    while (pages > 0) {
        struct pgd *pgd = get_pgd(map, get_dir(va));
        if (seg_dir(seg)) {
            pgd->cnt = 0;
            va += mmu_dir_size();
            pages -= mmu_page_in_dir();
        } else {
            do {
                uint8_t shift = (uint8_t)((va % mmu_dir_size()) / mmu_page_size());

                // страницы пересакаются с ядром - унмапить хитро :)
                if (pgd->kpgd) {
                    // TODO
                    unmap_kpage(pgd->pgt + shift, pgd, va);
                } else
                    unmap_page(pgd->pgt + shift);
                --pgd->cnt;
                va += mmu_page_size();
            } while (--pages && ((va % mmu_dir_size()) != 0));
        }

        if (!pgd->cnt) {
            if (!pgd->kpgd)
                unmap_dir(va);
            free_pgd(map->pgds, pgd, get_dir(va));
        }
    }
    seg_remove(map, seg);

    if (map != cur_map[cpu_get_core_id()].map && map->mmu_pool != MAP_NO_POOL) {
        release_mmutbl_pool(map->mmu_pool);
        map->mmu_pool = MAP_NO_POOL;
    }

    map_unlock(map);
}

int vm_map_lookup (struct mmap *map, void *adr, size_t size)
{
    if (!map || !size)
        return DISJOINT;

    if (!map->size)
        return DISJOINT;

    struct rb_node *node = rb_tree_search_neareqless(map->segs, (size_t)adr);
    if (!node)
        return DISJOINT;
    struct seg *seg = rb_node_get_data(node);
    if (seg->adr + seg->size < adr)
        return DISJOINT;
    if (seg->adr + seg->size >= adr + size)
        return COMPLETE;
    return PARTIAL;
}

static void free_mmu_pgd (struct rb_node *node)
{
    if (!node)
        return;
    struct rb_node *next_node = NULL;
    if (rb_tree_get_next(node, &next_node)) {
        free_mmu_pgd(next_node);
        next_node = NULL;
    }
    struct pgd *pgd = (struct pgd*)&(node->data);
    if (pgd->pgt)
        delete_pagetable(pgd->pgt);
    kfree(node);
    free_mmu_pgd(next_node);
}

static void free_map_segs (struct rb_node *node)
{
    if (!node)
        return;
    struct rb_node *next_node = NULL;
    if (rb_tree_get_next(node, &next_node)) {
        free_map_segs(next_node);
        next_node = NULL;
    }
    struct seg *seg = rb_node_get_data(node);
    seg_free(seg);
    kfree(seg);
    kfree(node);
    free_map_segs(next_node);
}

void vm_map_terminate (struct mmap *map)
{
    release_mmutbl_pool(map->mmu_pool);
    free_mmu_pgd(rb_tree_get_min(map->pgds));
    kfree(map->pgds);
    free_map_segs(rb_tree_get_min(map->segs));
    kfree(map->segs);
    kfree(map);
    --stat.maps;
}

struct mmap* vm_map_create ()
{
    struct mmap *map = kmalloc(sizeof(*map));
    map->pgds = kmalloc(sizeof(*map->pgds));
    rb_tree_init(map->pgds);
    rb_tree_set_mode(map->pgds, RBTREE_BY_KEY_VALUE);
    map->segs = kmalloc(sizeof(*map->segs));
    rb_tree_init(map->segs);
    rb_tree_set_mode(map->segs, RBTREE_BY_KEY_VALUE);
    map->size = 0;
    map->mmu_pool = MAP_NO_POOL;
    kobject_lock_init(&map->lock);
    ++stat.maps;
    return map;
}

struct seg* vm_seg_get (struct mmap *map, void *adr)
{
    if (!map)
        return NULL;
    struct rb_node *node = rb_tree_search(map->segs, (size_t) adr);
    if (!node)
        return NULL;
    struct seg *seg = rb_node_get_data(node);
    return seg;
}

int vm_seg_move (struct mmap *map_from, struct mmap *map_to, struct seg *seg)
{
    if (map_from == map_to)
        return ERROR(ERR_ILLEGAL_ARGS);

    struct rb_node *node = rb_tree_search(map_from->segs, (size_t) seg->adr);
    if (!node)
        return ERROR(ERR);

    unmap(map_from, seg);
    mmap(map_to, seg);
    seg->map = map_to;
    return OK;
}

static inline void set_share_cnt (struct rb_node *node, int val) { rb_node_set_data(node, (void*)val); }
static inline int get_share_cnt (struct rb_node *node) { return (int)rb_node_get_data(node); }
static void dec_share_cnt (struct rb_node *node) { rb_node_set_data(node,  rb_node_get_data(node) - 1); }
static void inc_share_cnt (struct rb_node *node) { rb_node_set_data(node, rb_node_get_data(node) + 1); }

int vm_seg_share (struct mmap *map_to, struct seg *seg)
{
    if (seg->map == map_to)
        return ERROR(ERR_ILLEGAL_ARGS);

    struct rb_node *node = rb_tree_search(seg->shared, (size_t)map_to);
    if (node) {
        inc_share_cnt(node);
    } else {
        struct rb_node *new_node = kmalloc(sizeof(*new_node));
        rb_node_init(new_node);
        rb_node_set_key(new_node, (size_t)map_to);
        set_share_cnt(new_node, 1);
        if (!seg->shared) {
            seg->shared = kmalloc(sizeof(*(seg->shared)));
            rb_tree_init(seg->shared);
            rb_tree_set_mode(seg->shared, RBTREE_BY_KEY_VALUE);
        }
        rb_tree_insert(seg->shared, new_node);
        mmap(map_to, seg);
    }
    return OK;
}

int vm_seg_unshare (struct mmap *map_from, struct seg *seg)
{
    struct rb_node *node = rb_tree_search(seg->shared, (size_t)map_from);
    if (!node)
        return ERROR(ERR);
    dec_share_cnt(node);
    if (get_share_cnt(node)) {
        return OK;
    }
    rb_tree_remove(seg->shared, node);
    kfree(node);
    unmap(map_from, seg);
    return OK;
}

/* Под стеки память выделяется с дополнительными граничными защитными страницами.
 Это страницы, которые непосредственно примыкают к блоку спереди и сзади.
 При получении свободных страниц у аллокатора они всегда недоступны по MMU.
 Поэтому если не добавить их в сегмент, они так и останутся недоступными
 и будут защищать стек от переполнения в любую сторону.
 */
void* vm_alloc_stack (struct mmap *map, size_t pages, bool only_kernel)
{
    void *adr = page_alloc(pages + 2); // +2 это граничные защитные страницы окружающие стек
    if (!adr)
        return NULL;

    adr += mmu_page_size();

    const mem_attributes_t stack_attr = {
        .type = MEM_TYPE_NORMAL,
        .exec = MEM_EXEC_NEVER,
        .os_access = MEM_ACCESS_RW,
        .process_access = only_kernel ? MEM_ACCESS_NO : MEM_ACCESS_RW,
        .inner_cached = MEM_CACHED_WRITE_BACK,
        .outer_cached = MEM_CACHED_WRITE_BACK,
        .shared = MEM_SHARED_OFF,
    };

    struct seg *seg = seg_create(map, adr, pages * mmu_page_size(), stack_attr);
    seg->type = SEG_STACK;
    mmap(map, seg);
    return adr;
}

int vm_map (struct process *proc, void *adr, size_t pages, mem_attributes_t attr)
{
    adr = page_alloc_fixed((size_t) adr, pages, attr.multu_alloc);
    if (!adr)
        return ERROR(ERR_NO_MEM);

    struct mmap *map = proc->mmap;
    mmap(map, seg_create(map, adr, pages * mmu_page_size(), attr));
    return OK;
}

void* vm_alloc (struct process *proc, size_t pages, mem_attributes_t attr)
{
    void *adr = page_alloc(pages);
    if (!adr)
        return NULL;

    struct mmap *map = proc->mmap;
    mmap(map, seg_create(map, adr, pages * mmu_page_size(), attr));
    return adr;
}

void* vm_alloc_align (struct mmap *map, size_t pages, size_t align, mem_attributes_t attr)
{
    void *adr = page_alloc_align(pages, align, 0);
    if (!adr)
        return NULL;

    mmap(map, seg_create(map, adr, pages * mmu_page_size(), attr));
    return adr;
}

static void vm_seg_unshare_all (struct seg *seg)
{
    if (!seg->shared)
        return;

    while (seg->shared->nodes) {
        struct rb_node *node = rb_tree_get_min(seg->shared);
        struct mmap *map = (struct mmap *) rb_node_get_key(node);
        vm_seg_unshare(map, seg);
    }
    kfree(seg->shared);
}

int vm_free (struct process *proc, void *adr)
{
    struct mmap *map = proc->mmap;
    struct rb_node *node = rb_tree_search(map->segs, (size_t) adr);
    if (!node)
        return ERROR(ERR);

    struct seg *seg = rb_node_get_data(node);
    if (seg->map != map) {
        return vm_seg_unshare(map, seg);
    }
    unmap(map, seg);
    vm_seg_unshare_all(seg);
    seg_free(seg);
    kfree(seg);
    --stat.segs;
    return OK;
}

static void load_map (struct mmap *map)
{
    if (!map)
        return;

    if (map == kmap)
        return;

    map_lock(map);
    int i = load_to_mmutbl_pool(map);
    cur_map[cpu_get_core_id()].map = map;
    cur_map[cpu_get_core_id()].pool = i;
    mmu_switch(get_mmutbl_pool(i));
    map_unlock(map);
}

static void unload_map (struct mmap *map)
{
    if (!map)
        return;

    if (map == kmap)
        return;

    map_lock(map);
    light_release_mmutbl_pool(cur_map[cpu_get_core_id()].pool);
    cur_map[cpu_get_core_id()].map = NULL;
    cur_map[cpu_get_core_id()].pool = MAP_NO_POOL;
    map_unlock(map);
}

void vm_switch (struct mmap *unload, struct mmap *load)
{
    if (unload == load)
        return;
    unload_map(unload);
    load_map(load);
    flush_tlb();
}

void vm_enable ()
{
    mmu_switch(get_mmutbl_pool(cpu_get_core_id()));
    mmu_enable();
    log_info("memory init\n\r");
}

static void pages_init ()
{
    page_allocator_init(mmu_page_size());
}

void vm_init ()
{
    kobject_lock_init(&mmulock);
    init_mmutbl_pool();

    for (int i = 0; i < NUM_CORE; i++) {
        cur_map[i].map = NULL;
        cur_map[i].pool = MAP_NO_POOL;
    }
    pages_init();
}

static int print_attr (char *buf, size_t max, mem_attributes_t attr)
{
    int size = 0;
    char *shared = "Shared";
    char *exec = "Exec";
    char *mem_type[] = {"Strongly-ordered", "Device", "Normal"};
    char *cached[] = {"WB_WA", "WT", "WR"};
    char *ap[] = {"RO", "RW"};
    char *security = "Security";
    char *multi_alloc = "Multi-alloc";

    size += snprintf(buf, max, "\t %s ", mem_type[attr.type]);
    if (attr.exec)
        size += snprintf(buf + size, max, "%s ", exec);
    if (attr.shared)
        size += snprintf(buf + size, max, "%s ", shared);
    if (attr.security)
        size += snprintf(buf + size, max, "%s ", security);
    if (attr.multu_alloc)
        size += snprintf(buf + size, max, "%s ", multi_alloc);
    if (attr.os_access != MEM_ACCESS_NO) {
        size += snprintf(buf + size, max, "OS:%s ", ap[attr.os_access - 1]);
    }
    if (attr.process_access != MEM_ACCESS_NO) {
        size += snprintf(buf + size, max, "Proc:%s ", ap[attr.process_access - 1]);
    }
    if (attr.inner_cached != MEM_CACHED_OFF) {
        size += snprintf(buf + size, max, "CacheI:%s ", ap[attr.inner_cached - 1]);
    }
    if (attr.outer_cached != MEM_CACHED_OFF) {
        size += snprintf(buf + size, max, "CacheO:%s ", ap[attr.outer_cached - 1]);
    }
    size += snprintf(buf + size, max, "\n\r");
    return size;
}



size_t print_map (char *buf, size_t max, struct mmap *map)
{
    if (!map)
        return -1;

    size_t size = snprintf(buf, max, "0x%p size = %d(0x%X)\n\r", map, map->size, map->size);
    struct rb_node *node = rb_tree_get_min(map->segs);
    while (node) {
        struct seg *seg = rb_node_get_data(node);
        size += snprintf(buf + size, max - size, "Segment: \n\r\t address = 0x%p,\n\r\t size = %d(0x%X),\n\r\t", seg->adr, seg->size, seg->size);
        size += print_attr(buf + size, max - size, seg->attr);
        node = rb_tree_get_nearlarger(node);
    }
    return size;
}

void vm_get_info (struct mem_info *info)
{
    info->page_size = mmu_page_size();
    info->heap.size = get_kheap_size();
    info->heap.free = get_kheap_free_size();
    info->map.size = kmap->size;
    info->map.segs = kmap->segs->nodes;
    info->allocated.size = get_page_dynamic_total() * mmu_page_size();
    info->allocated.free = get_page_dynamic_free() * mmu_page_size();
    info->mapped.size = get_page_fixed_total() * mmu_page_size();
    info->mapped.free = get_page_fixed_free() * mmu_page_size();
    info->maps = stat.maps;
    info->segs = stat.segs;
    info->pgts = stat.page_tables;
}

void map_kmap (struct process *proc, void *adr)
{
    struct seg *seg = vm_seg_get(proc->mmap, adr);
    mmap(kmap, seg);
}

void unmap_kmap (struct process *proc, void *adr)
{
    struct seg *seg = vm_seg_get(proc->mmap, adr);
    unmap(kmap, seg);
}
