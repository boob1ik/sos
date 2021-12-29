/* Куча реализована на бибилиотеке rtheap */

#include <arch.h>
#include <common/printf.h>
#include <common/syshalt.h>
#include <common/utils.h>
#include <common/log.h>
#include <rtheap.h>
#include <string.h>
#include "kmem.h"
#include "page.h"
#include "vm.h"
#include <syn/ksyn.h>
#include <stddef.h>

static void *kheap = NULL;
static kobject_lock_t kheaplock;

size_t get_kheap_size ()
{
    return heap_get_size(kheap);
}

size_t get_kheap_free_size ()
{
    return heap_get_freesize(kheap);
}

static inline void kheap_lock ()
{
    kobject_lock(&kheaplock);
}

static inline void kheap_unlock ()
{
    kobject_unlock(&kheaplock);
}

void* kmalloc (size_t size)
{
    if (!kheap)
        syshalt(SYSHALT_KMEM_ERROR);
    kheap_lock();
    void *adr = heap_alloc(kheap, size);
    kheap_unlock();
    if (adr == NULL)
        syshalt(SYSHALT_KMEM_NO_MORE);
    return adr;
}

void* kmalloc_aligned (size_t size, size_t align)
{
    if (!kheap)
        syshalt(SYSHALT_KMEM_ERROR);
    kheap_lock();
    void *adr = heap_alloc_aligned(kheap, size, align);
    kheap_unlock();
    if (adr == NULL)
        syshalt(SYSHALT_KMEM_NO_MORE);
    return adr;
}

void kfree (void *ptr)
{
    if (!kheap)
        syshalt(SYSHALT_KMEM_ERROR);
    kheap_lock();
    if (heap_free(kheap, ptr) != OK)
        syshalt(SYSHALT_KMEM_ERROR);
    kheap_unlock();
}

static void* kheap_add (size_t *size)
{
    *size = ALIGN(*size, mmu_dir_size());
    const mem_attributes_t kheap_page_attr = { HEAP_ATTR };
    void *adr = vm_alloc_align(kmap, *size / mmu_page_size(), mmu_dir_size(), kheap_page_attr);
    if (!adr)
        syshalt(SYSHALT_KMEM_NO_MORE);
    log_info("kheap extended: 0x%08lx bytes\n\r", *size);
    return adr;
}

void kmem_init ()
{
    extern char __heap_start__[], __heap_end__[];
    extern char __heap_ext_start__[], __heap_ext_end__[];
    kheap = __heap_start__;
    size_t size = (size_t)(__heap_end__ - __heap_start__);
    heap_flags_t f = {
        .autoextend_freesize = KMEM_AUTOEXTEND_FREESIZE,
        .rtenabled = 1,
    };
    if (heap_init(kheap, size, f, kheap_add) != OK)
        syshalt(SYSHALT_KMEM_ERROR);
    size = __heap_ext_end__ - __heap_ext_start__;
    if (size > 0) {
        if (heap_region_add(kheap, __heap_ext_start__, size) != OK)
            syshalt(SYSHALT_KMEM_ERROR);
    }
    kobject_lock_init(&kheaplock);
}

int print_kheap (char *buf)
{
    if (!kheap)
        return snprintf(buf, 20, "No kernel heap");

    return snprintf(buf, 20, "Kernel heap size=%d, free=%d", heap_get_size(kheap), heap_get_freesize(kheap));
}
