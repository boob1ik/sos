#include <os-libc.h>
PROCNAME("boot");

#include "boot.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#define BOOT_LOG_PATH         "boot/log"
#define BOOT_CONFIRM_PATH     "boot/confirm"
#define BOOT_CONFIRM_CODE     "booted"

struct msg_boot {
    unsigned long code;
};

#define DEBUG_ASSERT_SC(x) {if (x < 0) {err_t err = (err_t)x; asm volatile ("bkpt");}}

static struct proc_header const * const boot_procs_hdr[] = {
    (struct proc_header const * const )0x10200000, // driver manager
    // logger
    // secure
    NULL
};

static int uart_chid = ERR;
static int uart_conid = ERR;

static int log_chid = ERR;

void print (char *str)
{
    struct msg m = {
        .sys._ptr = NULL,
        .user.size = strlen(str),
        .user.data = str,
    };
    DEBUG_ASSERT_SC(os_send(uart_conid, &m, TIMEOUT_INFINITY, MSG_WAIT_COMPLETE));
}

static void boot_log ()
{
    struct connection_info info;
    DEBUG_ASSERT_SC(os_channel_wait_connection(log_chid, &info, TIMEOUT_INFINITY));
    DEBUG_ASSERT_SC(os_channel_complete_connection(log_chid, CONNECTION_ALLOW, 0));
    for (;;) {
        struct msg *m = NULL;
        int res = os_receive(log_chid, &m, TIMEOUT_INFINITY);
        if (res == ERR_DEAD)
            break;
        if (res != OK)
            asm volatile ("bkpt");
        DEBUG_ASSERT_SC(os_send(uart_conid, m, NO_WAIT, NO_FLAGS));
    }
}

static void start_console ()
{
    struct proc_header const * const console = (struct proc_header const * const )0x10100000;
    DEBUG_ASSERT_SC(spawnm(console, PRIO_DEFAULT, "dev/uart/2", NULL));
}

static int confirm_exec (int pid, int chid)
{
    struct connection_info info;
    DEBUG_ASSERT_SC(os_channel_wait_connection(chid, &info, TIMEOUT_INFINITY));

    if (info.pid != pid)
        return ERR;

    int pid_chid = os_channel_open(CHANNEL_PRIVATE, NULL, 4096, CHANNEL_SINGLE_CONNECTION | CHANNEL_AUTO_KILL);
    DEBUG_ASSERT_SC(os_channel_complete_connection(chid, CONNECTION_TO_PRIVATE, pid_chid));

    struct msg *m = NULL;
    if (os_receive(pid_chid, &m, TIMEOUT_INFINITY) == OK) {
        char *confirm = m->user.data;
        if (confirm) {
            if (memcmp(confirm, BOOT_CONFIRM_CODE, strlen(confirm)) == 0) {
                DEBUG_ASSERT_SC(os_channel_close(pid_chid));
                return OK;
            }
        }
    }
    return ERR;
}

static int exec_proc (struct proc_header const * const hdr, int confirm_chid)
{
    int pid = spawnm(hdr, PRIO_DEFAULT, BOOT_CONFIRM_PATH, BOOT_CONFIRM_CODE, BOOT_LOG_PATH, NULL);
    DEBUG_ASSERT_SC(pid);
    if (confirm_exec(pid, confirm_chid) != OK) {
        asm volatile ("bkpt");
    }
    return pid;
}

int main ()
{
    int confirm_chid = os_channel_open(CHANNEL_PROTECTED, BOOT_CONFIRM_PATH, 4096, CHANNEL_SINGLE_CONNECTION);
    DEBUG_ASSERT_SC(confirm_chid);

    struct proc_header const * const drv_uart_hdr = (struct proc_header const * const )0x10300000;
    int uart_pid = spawnm(drv_uart_hdr, PRIO_MAX, "boot", "2", BOOT_CONFIRM_PATH, BOOT_CONFIRM_CODE, NULL);
    if (confirm_exec(uart_pid, confirm_chid) == OK) {
        uart_chid = os_channel_open(CHANNEL_PRIVATE, NULL, 4096, CHANNEL_SINGLE_CONNECTION | CHANNEL_AUTO_KILL);
        DEBUG_ASSERT_SC(uart_chid);

        uart_conid = os_connection_open("boot/uart/2", uart_chid, TIMEOUT_INFINITY);
        DEBUG_ASSERT_SC(uart_conid);
    } else
        asm volatile ("bkpt");

    print("Boot SABRE Lite board ("__DATE__" - "__TIME__")\n\r");
    print("Booting processes ...\n\r");

    log_chid = os_channel_open(CHANNEL_PROTECTED, BOOT_LOG_PATH, 4096, CHANNEL_SINGLE_CONNECTION);
    DEBUG_ASSERT_SC(log_chid);
    struct thread_attr attributes = {
        .entry = boot_log,
        .arg = NULL,
        .tid = 0,
        .stack_size = 4096,
        .ts = 1,
        .flags = NO_FLAGS,
        .type = THREAD_TYPE_DETACHED,
    };
    int log_tid = os_thread_create(&attributes);
    DEBUG_ASSERT_SC(os_thread_run(log_tid));

    for (int i = 0; boot_procs_hdr[i]; i++) {
        exec_proc(boot_procs_hdr[i], confirm_chid);
    }

    DEBUG_ASSERT_SC(os_channel_close(log_chid));
    DEBUG_ASSERT_SC(os_channel_close(confirm_chid));

    print("All processes booted\n\r");

    DEBUG_ASSERT_SC(os_connection_close(uart_conid));
    DEBUG_ASSERT_SC(os_channel_close(uart_chid));
    os_thread_sleep(TIMEOUT_1_SEC * 3);

    start_console();
    return OK;
}
