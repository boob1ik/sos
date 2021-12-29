#include <os.h>
#include <rtheap.h>
#include "pheap.h"
#include <reent.h>

void *malloc (size_t size)
{
    void *ptr = get_proc_heap();
    if (ptr == NULL) {
        return NULL;
    }
    if (proc_heap_lock() != OK) {
        return NULL;
    }
    ptr = heap_alloc(ptr, size);
    proc_heap_unlock();
    return ptr;
}

void *_malloc_r(struct _reent *r, size_t s) {
    return malloc(s);
}

//void *_sbrk(size_t s) {
//    return NULL;
//}
