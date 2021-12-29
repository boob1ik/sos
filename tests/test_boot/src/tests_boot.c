#include <os.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

void *test_procs_addr[] = {
//    (void*)0x10100000,         // test-mem
//    (void*)0x10200000,         // test-proc
//    (void*)0x10300000,         // test-thread
//    (void*)0x10400000,         // test-syn
    (void*)0x10500000,         // test-msg
//    (void*)0x10600000,         // test-irq
    NULL
};
int main (int argc, char *argv[])
{
    struct proc_attr pattr = {
            .pid = -1,
            .tid = -1,
            .prio = PRIO_MAX,
            .argv = NULL,
            .arglen = 0,
        };

    for (int i = 0; ; i++) {
        struct proc_header *phdr = (struct proc_header *)test_procs_addr[i];
        if (!phdr)
            break;
        if (os_proc_create(phdr, &pattr) >= 0) {
            os_thread_join(pattr.tid);
        } else {
            while(1);
        }
    }
    asm volatile ("bkpt"); // все тесты ОК
    return 0;
}
