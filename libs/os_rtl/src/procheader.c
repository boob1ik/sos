#include <os.h>
#include <os-libc.h>
#include <string.h>
#include "pheap.h"

const register void *thread_static_storage asm ("r9");

extern int main (int argc, char *argv[]);
extern char __text_start__[], __text_size__[];
extern char __rodata_start__[], __rodata_size__[];
extern char __data_start__[], __data_size__[];
extern char __bss_start__[], __bss_size__[];
extern struct proc_header __attribute__ ((section (".proc_header"))) __boot_proc_header__;

extern char __proc_name[];

void __entry(size_t arglen, void *argv, int argtype) {
    register long long *p = (long long *)__bss_start__;
    register long long *end = (long long *)((size_t)__bss_start__ + (size_t)__bss_size__);
    register long long zero = 0;
    if(p != end) {
        do {
            *p++ = zero;
        } while(p < end);
    }
    proc_heap_init();
    switch(argtype) {
    case PROC_ARGTYPE_STRING_ARRAY:
        if(arglen > 0) {
            int argc = 0;
            char **ptr = argv;
            while(ptr[argc] != NULL) {
                argc++;
                if(argc > VA_LIST_ARGS_LIMIT) {
                    main(0, NULL);
                    return;
                }
            }
            main(argc, argv);
        } else {
            main(0, NULL);
        }
        break;
    case PROC_ARGTYPE_BYTE_ARRAY:
    default:
        main(arglen, argv);
        break;
    }
}

struct proc_header __attribute__ ((section (".proc_header"))) __boot_proc_header__ =
        {
            .magic = PROC_HEADER_MAGIC, //
            .type = 0, //
            .pathname = __proc_name, //
            .entry = __entry, //
            .stack_size = DEFAULT_PAGE_SIZE, //
            .proc_seg_cnt = 4, //
            .segs = {
                {
                    .adr = __text_start__, //
                    .size = (size_t) __text_size__, //
                    .attr = { //
                                .shared = MEM_SHARED_OFF, //
                                .exec = MEM_EXEC_ON, //
                                .type = MEM_TYPE_NORMAL, //
                                .inner_cached = MEM_CACHED_WRITE_BACK, //
                                .outer_cached = MEM_CACHED_WRITE_BACK, //
                                .process_access = MEM_ACCESS_RW, //
                                .os_access = MEM_ACCESS_RW //
                            }//
                }, //
                {
                    .adr = __data_start__, //
                    .size = (size_t) __data_size__, //
                    .attr = { //
                                .shared = MEM_SHARED_OFF, //
                                .exec = MEM_EXEC_NEVER, //
                                .type = MEM_TYPE_NORMAL, //
                                .inner_cached = MEM_CACHED_WRITE_BACK, //
                                .outer_cached = MEM_CACHED_WRITE_BACK, //
                                .process_access = MEM_ACCESS_RW, //
                                .os_access = MEM_ACCESS_RW //
                            }//
                }, //
                {
                    .adr = __rodata_start__, //
                    .size = (size_t) __rodata_size__, //
                    .attr = { //
                                .shared = MEM_SHARED_OFF, //
                                .exec = MEM_EXEC_NEVER, //
                                .type = MEM_TYPE_NORMAL, //
                                .inner_cached = MEM_CACHED_WRITE_BACK, //
                                .outer_cached = MEM_CACHED_WRITE_BACK, //
                                .process_access = MEM_ACCESS_RW, //
                                .os_access = MEM_ACCESS_RW //
                            }//
                }, //
                {
                    .adr = __bss_start__, //
                    .size = (size_t) __bss_size__, //
                    .attr = { //
                                .shared = MEM_SHARED_OFF, //
                                .exec = MEM_EXEC_NEVER, //
                                .type = MEM_TYPE_NORMAL, //
                                .inner_cached = MEM_CACHED_WRITE_BACK, //
                                .outer_cached = MEM_CACHED_WRITE_BACK, //
                                .process_access = MEM_ACCESS_RW, //
                                .os_access = MEM_ACCESS_RW //
                            }//
                } } };
