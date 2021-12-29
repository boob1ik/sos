#include "mmutbl_pool.h"
#include <arch.h>
#include <common/syshalt.h>

#define POOL_SIZE             4

size_t get_mmutbl_pool_size ()
{
    return POOL_SIZE;
}

static kobject_lock_t lock;

/* Для работы MMU используется пул таблиц L1.
 При освобождении слота таблица не очищается по дереву связанной карты,
 а лишь помечается незанятой. При выделении слота сначала ищется слот
 с уже загруженной картой, потом свободный слот, где эта карта была
 Если такого нет, первый свободный слот очищает связанную с ним L1
 и загружает требуемую.
 */
static struct {
    bool busy;
    struct mmap *map;
    uint32_t *tbl;
} mmutbl_pool[POOL_SIZE];

static int mmutbl_pool_cnt = 0;

static inline void pool_lock ()
{
    kobject_lock(&lock);
}

static inline void pool_unlock ()
{
    kobject_unlock(&lock);
}

static void map_tbl (struct rb_tree *tree, uint32_t *tbl)
{
    for (struct rb_node *node = rb_tree_get_min(tree); node; node = rb_tree_get_nearlarger(node)) {
        uint32_t *entry = tbl + rb_node_get_key(node);
        struct pgd *pgd = (struct pgd *)(&node->data);
        *entry = pgd->entry;
    }
}

static void unmap_tbl (struct rb_tree *tree, uint32_t *tbl)
{
    for (struct rb_node *node = rb_tree_get_min(tree); node; node = rb_tree_get_nearlarger(node)) {
        uint32_t *entry = tbl + rb_node_get_key(node);
        struct pgd *pgd = (struct pgd *)(&node->data);
        if (pgd->kpgd)
            *entry = pgd->kpgd->entry;
        else
            mmu_set_fault(entry);
    }
}

void init_mmutbl_pool ()
{
    extern char __mmu_tbl_start__[], __mmu_tbl_end__[];
    kobject_lock_init(&lock);
    pool_lock();
    bzero(__mmu_tbl_start__, __mmu_tbl_end__ - __mmu_tbl_start__);
    for (int i = 0; i < POOL_SIZE; i++) {
        mmutbl_pool[i].busy = false;
        mmutbl_pool[i].map = NULL;
        mmutbl_pool[i].tbl = (uint32_t *)(__mmu_tbl_start__ + i * mmu_dirtable_size());
    }
    pool_unlock();
}

static void set_mmutbl_pool (int slot, struct mmap *map)
{
    mmutbl_pool[slot].map = map;
    mmutbl_pool[slot].busy = true;
    map_tbl(map->pgds, mmutbl_pool[slot].tbl);
    mmutbl_pool_cnt++;
    map->mmu_pool = slot;
}

void light_release_mmutbl_pool (int slot)
{
    if (slot < 0 || slot > POOL_SIZE)
        syshalt(SYSHALT_OOPS_ERROR);
    pool_lock();
    mmutbl_pool[slot].busy = false;
    pool_unlock();
}

void release_mmutbl_pool (int slot)
{
    if (slot < 0 || slot > POOL_SIZE)
        syshalt(SYSHALT_OOPS_ERROR);
    pool_lock();
    if (mmutbl_pool[slot].map) {
        unmap_tbl(mmutbl_pool[slot].map->pgds, mmutbl_pool[slot].tbl);
        mmutbl_pool[slot].map = NULL;
        mmutbl_pool[slot].busy = false;
        mmutbl_pool_cnt--;
    }
    pool_unlock();
}

int load_to_mmutbl_pool (struct mmap *map)
{
    pool_lock();
    if (map->mmu_pool != MAP_NO_POOL) {
        // карта еще закеширована в пуле - вернемся на нее
        if (!mmutbl_pool[map->mmu_pool].busy) {
            mmutbl_pool[map->mmu_pool].busy = true;
        }
    } else {
        // проверим нет ли "чистого" пула
        if (mmutbl_pool_cnt < POOL_SIZE) {
            for (int i = 0; i < POOL_SIZE; i++) {
                if (!mmutbl_pool[i].busy && !mmutbl_pool[i].map) {
                    set_mmutbl_pool(i, map);
                    break;
                }
            }
        } else {
            // для "цикличного" занятия частично свободного пула
            static unsigned int start_ind = 0;

            // карты нигде не было, и чистых пулов нет
            // надо размещаться затирая любой свободный пул

            // не загружаемся сразу на место предыдущей загруженной
            static struct mmap *prev_map = NULL;
            if (mmutbl_pool[start_ind].map == prev_map)
                start_ind++;

            for (int j = 0, i = start_ind % POOL_SIZE; j < POOL_SIZE; j++, i++) {
                if (i >= POOL_SIZE)
                    i = 0;

                if (mmutbl_pool[i].busy)
                    continue;

                release_mmutbl_pool(i);
                set_mmutbl_pool(i, map);
                prev_map = map;
                start_ind++;
                break;
            }
        }
    }
    pool_unlock();

    if (map->mmu_pool == MAP_NO_POOL)
        syshalt(SYSHALT_OOPS_ERROR);

    return map->mmu_pool;
}

uint32_t* get_mmutbl_pool (int slot)
{
    return mmutbl_pool[slot].tbl;
}
