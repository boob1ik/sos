#include <os.h>
#include <string.h>
#include <stdint.h>
#include <stddef.h>

#define PROC_SRV                   0x10510000
#define PROC_CLIENT                0x10520000
//#define PROC_CLIENT_SEND_MEM       0x10530000
//#define PROC_CLIENT_SHARE_MEM      0x10540000

static void error (char *str)
{
    while (1) asm volatile ("bkpt");
}
int main (int argc, char *argv[])
{
    /*
    - PUBLIC server - PRIVATE client
    - PUBLIC server (PRIVATE) - PRIVATE client
    - PUBLIC server (redirect PROTECTED driver (PRIVATE)) - PRIVATE client

    */

    struct proc_attr pattr_srv = {
        .prio = PRIO_DEFAULT - 1,
        .argv = NULL,
        .arglen = 0,
    };
    if (os_proc_create((struct proc_header *)PROC_SRV, &pattr_srv) < 0) {
        error("con not create server msg proc");
        return -1;
    }

    struct proc_attr pattr_client = {
        .prio = PRIO_DEFAULT,
        .argv = NULL,
        .arglen = 0,
    };

    if (os_proc_create((struct proc_header *)PROC_CLIENT, &pattr_client) < 0) {
        error("con not create client msg proc");
        return -1;
    }

    if (os_thread_join(pattr_client.tid) < 0) {
        error("con not join client msg proc");
        return -1;
    }

//    if (os_proc_create((struct proc_header *)PROC_CLIENT_SEND_MEM, &pattr_client) < 0) {
//        error("con not create client sm msg proc");
//        return -1;
//    }
//
//    if (os_thread_join(pattr_client.tid) < 0) {
//        error("con not join client msg proc");
//        return -1;
//    }
//
//    if (os_proc_create((struct proc_header *)PROC_CLIENT_SHARE_MEM, &pattr_client) < 0) {
//        error("con not create client sm msg proc");
//        return -1;
//    }
//
//    if (os_thread_join(pattr_client.tid) < 0) {
//        error("con not join client msg proc");
//        return -1;
//    }

    if (os_proc_kill(pattr_srv.pid) < 0)
        error("con not kill server msg proc");
        return -1;

    return 0;
}
