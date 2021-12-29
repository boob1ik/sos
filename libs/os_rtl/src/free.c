#include <os.h>
#include <rtheap.h>
#include "pheap.h"

void free (void *p)
{
    void *heap = get_proc_heap();
    if (heap == NULL) {
        return;
    }
    if (proc_heap_lock() != OK) {
        return;
    }
    heap_free(heap, p);
    proc_heap_unlock();
}

