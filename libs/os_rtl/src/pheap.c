#include <os.h>
#include <rtheap.h>

extern struct proc_header __boot_proc_header__;
extern char __bss_start__[];

static void *proc_heap = NULL;
static syn_t syn_heap;
static mem_attributes_t default_heap_attr = {
    .shared = MEM_SHARED_OFF, //
    .exec = MEM_EXEC_NEVER, //
    .type = MEM_TYPE_NORMAL, //
    .inner_cached = MEM_CACHED_WRITE_BACK, //
    .outer_cached = MEM_CACHED_WRITE_BACK, //
    .process_access = MEM_ACCESS_RW, //
    .os_access = MEM_ACCESS_RW //
        };

static void *proc_heap_extend (size_t *size_io)
{
    void *newregion = NULL;
    size_t newsize = *size_io;
    newsize = (newsize + DEFAULT_PAGE_SIZE - 1) / DEFAULT_PAGE_SIZE;
    newregion = os_malloc(newsize, default_heap_attr, 0);
    if (newregion != NULL) {
        *size_io = newsize * DEFAULT_PAGE_SIZE;
    }
    return newregion;
}

int proc_heap_init ()
{
    int res;
    void *heap;
    if (proc_heap) {
        return ERR_BUSY;
    }
    // mem attributes - from __bss_start__ attributes
    for (int i = 0; i < __boot_proc_header__.proc_seg_cnt; i++) {
        if (__boot_proc_header__.segs[i].adr == __bss_start__) {
            default_heap_attr = __boot_proc_header__.segs[i].attr;
            break;
        }
    }
    heap = os_malloc(1, default_heap_attr, 0);
    if (heap == NULL) {
        return ERR_NO_MEM;
    }
    heap_flags_t mode = {
        .autoextend_freesize = 0, .rtenabled = 1, .alloc_firstfit = 0 };
    res = heap_init(heap, DEFAULT_PAGE_SIZE, mode, proc_heap_extend);
    if (res != OK) {
        return res;
    }
    syn_heap.type = MUTEX_TYPE_PLOCAL;
    syn_heap.pathname = NULL;
    res = os_syn_create(&syn_heap);
    if (res != OK) {
        os_mfree(heap);
        return res;
    }
    proc_heap = heap;
    return OK;
}

void * get_proc_heap ()
{
    return proc_heap;
}

int proc_heap_lock ()
{
    if (proc_heap == NULL) {
        return ERR_ACCESS_DENIED;
    }
    return os_syn_wait(syn_heap.id, TIMEOUT_INFINITY);
}

int proc_heap_unlock ()
{
    if (proc_heap == NULL) {
        return ERR_ACCESS_DENIED;
    }
    return os_syn_done(syn_heap.id);
}
