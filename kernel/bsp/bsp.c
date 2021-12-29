#include <arch.h>
#include <common/printf.h>
#include <mem\kmem.h>
#include <mem\vm.h>
#include <common\syshalt.h>
#include <common\log.h>
#include <string.h>
#include <proc.h>
#include "aips.h"
#include "uart.h"
#include "ccm_pll.h"
#include "registers/regsuart.h"
#include <common\namespace.h>
#include "mem\page.h"
#include "mem\vm.h"

static const mem_attributes_t kmem_attr_device_rw = {
    .shared = MEM_SHARED_OFF,
    .exec = MEM_EXEC_NEVER,
    .type = MEM_TYPE_DEVICE,
    .inner_cached = MEM_CACHED_OFF,
    .outer_cached = MEM_CACHED_OFF,
    .process_access = MEM_ACCESS_NO,
    .os_access = MEM_ACCESS_RW,
    .security = MEM_SECURITY_OFF,
};

#ifndef NO_BOOT

#define BOOT_PROC_NUM   1

const struct proc_header *hdr_ptrs[BOOT_PROC_NUM] = {
    (struct proc_header *) 0x10000000ul };

struct proc_header *boot_hdr_tmp[BOOT_PROC_NUM];

#endif

// Работаем без! включенного MMU, разрешен только kmalloc в пределах начального размера кучи ядра
int board_init ()
{
    // cpu_init();
    ccm_pll_init();
    ccm_init();
    aips_init();
    uart_init(HW_UART2, 115200, PARITY_NONE, STOPBITS_ONE, EIGHTBITS, FLOWCTRL_OFF);
    set_log_target(LOG_TARGET_UART2);

    // здесь необходимо инициализировать память DDR и др. ресурсы, которые требуют начальной настройки
#ifndef NO_BOOT
    // здесь копируем заголовок и его данные процесса-загрузчика
    // (и других процессов если есть) в область кучи ядра, так как они могут находится в памяти,
    // которая не будет доступна после включения MMU
    // (фнукция proc_init предполагает что эти данные доступны на момент вызова и не перекрываются с
    // целевой картой памяти, то есть не лежат по ее адресам)
    size_t seglen;
    size_t pathname_length = 0;
    for (int i = 0; i < BOOT_PROC_NUM; i++) {
        if( ((struct proc_header *) hdr_ptrs[i])->magic != PROC_HEADER_MAGIC ) {
            continue;
        }
        if(hdr_ptrs[i]->pathname != NULL) {
            pathname_length = strnlen(hdr_ptrs[i]->pathname, PATHNAME_MAX_LENGTH - PROCS_PATHNAME_ADD_LEN);
            if(pathname_length == PATHNAME_MAX_LENGTH - PROCS_PATHNAME_ADD_LEN) {
                continue;
            }
        }

        seglen = sizeof(struct proc_seg) * hdr_ptrs[i]->proc_seg_cnt;
        boot_hdr_tmp[i] = (struct proc_header *)kmalloc(sizeof(struct proc_header) + seglen + pathname_length + PROCS_PATHNAME_ADD_LEN + 16);
        memcpy(boot_hdr_tmp[i], hdr_ptrs[i], sizeof(struct proc_header) + seglen);
        boot_hdr_tmp[i]->pathname = (void *)boot_hdr_tmp[i] + sizeof(struct proc_header) + seglen + 8;
        if(pathname_length > 0) {
            // скопируем строку pathname, убирая лишние не обязательные разделители вначале и в конце, если есть;
            // при этом, если был только один или два разделителя, они будут заменены на пустую строку
            if( (hdr_ptrs[i]->pathname[0] == NAMESPACE_DEFAULT_SEPARATOR) ||
                    (hdr_ptrs[i]->pathname[0] == NAMESPACE_DEFAULT_ALT_SEPARATOR) ) {
                strcpy(boot_hdr_tmp[i]->pathname, hdr_ptrs[i]->pathname + 1);
                if( (pathname_length > 1) && ((hdr_ptrs[i]->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_SEPARATOR) ||
                        (hdr_ptrs[i]->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_ALT_SEPARATOR)) ) {
                    boot_hdr_tmp[i]->pathname[pathname_length - 2] = '\0';
                }
            } else {
                strcpy(boot_hdr_tmp[i]->pathname, hdr_ptrs[i]->pathname);
                if( (hdr_ptrs[i]->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_SEPARATOR) ||
                        (hdr_ptrs[i]->pathname[pathname_length - 1] == NAMESPACE_DEFAULT_ALT_SEPARATOR) ) {
                    boot_hdr_tmp[i]->pathname[pathname_length - 1] = '\0';
                }
            }
        } else {
            boot_hdr_tmp[i]->pathname[0] = '\0';
        }

    }
#endif

    return OK;
}

enum expansion_mem_type {
    MEM_MAP,     //!< выделение через vm_map
    MEM_ALLOC,   //!< выделение через vm_alloc, vm_alloc_align, vm_alloc_stack
};

static void memory_expansion (uint32_t start_adr, uint32_t end_adr, enum expansion_mem_type mtype)
{
    uint32_t pages = ((end_adr - start_adr) + 1) / mmu_page_size();
    switch (mtype) {
    case MEM_MAP:
        page_allocator_add((void*) start_adr, pages, true);
        break;
    case MEM_ALLOC:
        page_allocator_add((void*) start_adr, pages, false);
        break;
    default:
        syshalt(SYSHALT_OOPS_ERROR);
    }
}

static void kernel_memory_init ()
{
    extern char __kernel_mem_start__[], __kernel_mem_end__[];
    extern char __kheap_ext_mem_start__[], __kheap_ext_mem_end__[];
    extern char __mmu_mem_start__[], __mmu_mem_end__[];

    memory_expansion((uint32_t)__kernel_mem_start__, (uint32_t)__kernel_mem_end__, MEM_MAP);
    memory_expansion((uint32_t)__kheap_ext_mem_start__, (uint32_t)__kheap_ext_mem_end__, MEM_MAP);
    memory_expansion((uint32_t)__mmu_mem_start__, (uint32_t)__mmu_mem_end__, MEM_MAP);
}

static void periphery_memory_init ()
{
    // Добавляем к разрешенным областям памяти для возможного захвата ядром,
    // драйверами и др. процессами

    // Внимание !!! Перед этим нужно обеспечить нормальную работу добавляемой памяти
    memory_expansion(0x00000000, 0x00018fff, MEM_MAP);  // Boot ROM
//    memory_expansion((void *) 0x100000, 4);              // CAAM (16K secure RAM)
//    memory_expansion((void *) 0x120000, 9);              // HDMI
//    memory_expansion((void *) 0x134000, 4);              // GPU 2D (GC320)
    memory_expansion(0x00A00000, 0x00A02FFF, MEM_MAP);  // ARM MP + PL310
    memory_expansion(0x02000000, 0x021F3FFF, MEM_MAP);  // AIPS-1, AIPS-2
//    memory_expansion((void *) 0x2204000, 4);             // OpenVG (GC355)
//    memory_expansion((void *) 0x2600000, 2000);          // IPU-1, IPU-2
//    memory_expansion((void *) 0x8000000, 32000);         // EIM - Memory
}

static void mapped_memory_init ()
{
    extern char __empty_mem_start__[];//, __empty_mem_end__[];
    memory_expansion((uint32_t)__empty_mem_start__, (uint32_t)__empty_mem_start__ + 0xFFFFFF, MEM_MAP);
}

static void alloc_memory_init ()
{
    extern char __empty_mem_start__[], __empty_mem_end__[];
    memory_expansion((uint32_t)(__empty_mem_start__ + 0x1000000), (uint32_t)__empty_mem_end__, MEM_ALLOC);
}

int board_mem_init ()
{
    kernel_memory_init();
    periphery_memory_init();
    mapped_memory_init();
    alloc_memory_init();
    return OK;
}

void kmap_init ()
{
    kmap = vm_map_create();

    {
        extern char __mmu_tbl_start__[], __mmu_tbl_end__[];
        mem_attributes_t attr = {
            .shared = MEM_SHARED_OFF,
            .exec = MEM_EXEC_NEVER,
            .type = MEM_TYPE_NORMAL,
            .inner_cached = ARM_TTBR_INNER_CACHED,
            .outer_cached = ARM_TTBR_OUTER_CACHED,
            .process_access = MEM_ACCESS_NO,
            .os_access = MEM_ACCESS_RW,
        };
        vm_map(&kproc, __mmu_tbl_start__, (__mmu_tbl_end__ - __mmu_tbl_start__) / PAGE_SIZE, attr);
    }

    {
        extern char __heap_start__[], __heap_end__[];
        extern char __heap_ext_start__[], __heap_ext_end__[];
        mem_attributes_t attr = { HEAP_ATTR };
        vm_map(&kproc, __heap_start__, (__heap_end__ - __heap_start__) / PAGE_SIZE, attr);
        vm_map(&kproc, __heap_ext_start__, (__heap_ext_end__ - __heap_ext_start__) / PAGE_SIZE, attr);
    }

    {
        extern char __stacks_start[], __stacks_end[];
        mem_attributes_t attr = {
            .shared = MEM_SHARED_OFF,
            .exec = MEM_EXEC_NEVER,
            .type = MEM_TYPE_NORMAL,
            .inner_cached = MEM_CACHED_WRITE_BACK,
            .outer_cached = MEM_CACHED_WRITE_BACK,
            .process_access = MEM_ACCESS_NO,
            .os_access = MEM_ACCESS_RW,
        };
        vm_map(&kproc, __stacks_start, (__stacks_end - __stacks_start) / PAGE_SIZE, attr);
    }

    {
        extern char __text_start__[], __text_end__[];
        mem_attributes_t attr = {
            .shared = MEM_SHARED_OFF,
            .exec = MEM_EXEC_ON,
            .type = MEM_TYPE_NORMAL,
            .inner_cached = MEM_CACHED_WRITE_BACK,
            .outer_cached = MEM_CACHED_WRITE_BACK,
            .process_access = MEM_ACCESS_NO,
            .os_access = MEM_ACCESS_RO,
        };
        vm_map(&kproc, __text_start__, (__text_end__ - __text_start__) / PAGE_SIZE, attr);
    }

    {
        extern char __rodata_start__[], __rodata_end__[];
        mem_attributes_t attr = {
            .shared = MEM_SHARED_OFF,
            .exec = MEM_EXEC_NEVER,
            .type = MEM_TYPE_NORMAL,
            .inner_cached = MEM_CACHED_WRITE_BACK,
            .outer_cached = MEM_CACHED_WRITE_BACK,
            .process_access = MEM_ACCESS_NO,
            .os_access = MEM_ACCESS_RO,
        };
        vm_map(&kproc, __rodata_start__, (__rodata_end__ - __rodata_start__) / PAGE_SIZE, attr);
    }

    {
        extern char __data_start__[], __data_end__[];
        mem_attributes_t attr = {
            .shared = MEM_SHARED_OFF,
            .exec = MEM_EXEC_NEVER,
            .type = MEM_TYPE_NORMAL,
            .inner_cached = MEM_CACHED_WRITE_BACK,
            .outer_cached = MEM_CACHED_WRITE_BACK,
            .process_access = MEM_ACCESS_NO,
            .os_access = MEM_ACCESS_RW,
        };
        vm_map(&kproc, __data_start__, (__data_end__ - __data_start__) / PAGE_SIZE, attr);
    }

    {
        extern char __bss_start__[], __bss_end__[];
        mem_attributes_t attr = {
            .shared = MEM_SHARED_OFF,
            .exec = MEM_EXEC_NEVER,
            .type = MEM_TYPE_NORMAL,
            .inner_cached = MEM_CACHED_WRITE_BACK,
            .outer_cached = MEM_CACHED_WRITE_BACK,
            .process_access = MEM_ACCESS_NO,
            .os_access = MEM_ACCESS_RW,
        };
        vm_map(&kproc, __bss_start__, (__bss_end__ - __bss_start__) / PAGE_SIZE, attr);
    }

    {
        extern char __user_text_common_start[], __user_text_common_end[];
        mem_attributes_t attr = {
            .shared = MEM_SHARED_OFF,
            .exec = MEM_EXEC_ON,
            .type = MEM_TYPE_NORMAL,
            .inner_cached = MEM_CACHED_WRITE_BACK,
            .outer_cached = MEM_CACHED_WRITE_BACK,
            .process_access = MEM_ACCESS_RO,
            .os_access = MEM_ACCESS_RO,
        };
        vm_map(&kproc, __user_text_common_start, (__user_text_common_end - __user_text_common_start) / PAGE_SIZE, attr);
    }

    vm_map(&kproc, (void*)0x00A00000, 3, kmem_attr_device_rw); // ARM MP + PL310
    vm_map(&kproc, (void*)0x0207C000, 4, kmem_attr_device_rw); // AIPS-1 configuration
    vm_map(&kproc, (void*)0x02140000, 33, kmem_attr_device_rw); // ARM MP/DAP
    vm_map(&kproc, (void*)0x0217C000, 4, kmem_attr_device_rw); // AIPS-2 configuration
    vm_map(&kproc, (void*)0x020d0000, 4, kmem_attr_device_rw); // EPIT1
    vm_map(&kproc, (void*)0x021E8000, 4, kmem_attr_device_rw); // UART2

#ifdef DEBUG
    char *buf = kmalloc(4096);
    char *_buf = buf;
    log_info("Kernel map: ");
    size_t size = print_map(buf, 4096, kmap);
    while (size) {
        size_t len = log_info(buf);
        buf += len;
        size -= len;
    }
    kfree(_buf);
#endif
}

// Работаем с MMU
int board_boot_init ()
{
    l1_cache_enable();
    l2_cache_enable();
#ifndef NO_BOOT
    struct proc_attr attr = {
        .pid = -1,
        .tid = 0,
        .prio = PRIO_MAX - 1,
        .argtype = PROC_ARGTYPE_BYTE_ARRAY,
        .argv = NULL,
        .arglen = 0
    };
    for (int i = 0; i < BOOT_PROC_NUM; i++) {
        if (proc_init(boot_hdr_tmp[i], &attr, NULL) != OK) {
            syshalt(SYSHALT_BOOT_NOT_EXIST);
        }
    }
#endif
    return OK;
}

int board_post_init ()
{
    set_log_target(LOG_TARGET_BUFFER);
    vm_free(&kproc, (void*)0x021E8000); // UART2
    return OK;
}

