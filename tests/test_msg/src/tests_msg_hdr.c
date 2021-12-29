#include <os.h>

extern int main (int argc, char *argv[]);
extern char __text_start__[], __text_size__[];
extern char __rodata_start__[], __rodata_size__[];
extern char __data_start__[], __data_size__[];
extern char __bss_start__[], __bss_size__[];

void _start (int argc, char *argv[])
{
    register long long *p = (long long *)__bss_start__;
    register long long *end = (long long *)((size_t)__bss_start__ + (size_t)__bss_size__);
    register long long zero = 0;
    if (p != end) {
        do {
            *p++ = zero;
        } while(p < end);
    }
    main(argc, argv);
}

struct proc_header __attribute__ ((section (".proc_header"))) __proc_header__ =
        {
            .magic = PROC_HEADER_MAGIC, //
            .type = 0, //
            .name = "OS test msg", //
            .entry = _start, //
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
