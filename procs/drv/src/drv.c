#include <os-libc.h>
PROCNAME("driver_manager");

#include <string.h>
#include <stdlib.h>

#define DEBUG_ASSERT_SC(x) {if (x < 0) {err_t err = (err_t)x; asm volatile ("bkpt");}}

#define CONFIRM_DEV_PATH     "dev_confirm"
#define CONFIRM_DEV_CODE     "ready"

static void send_confirm_boot (char *path, char *code)
{
    int parent = os_connection_open(path, NO_REPLY, TIMEOUT_INFINITY);
    DEBUG_ASSERT_SC(parent);

    struct msg m = {
        .sys._ptr = NULL,
        .user.size = strlen(code),
        .user.data = code,
    };
    DEBUG_ASSERT_SC(os_send(parent, &m, TIMEOUT_INFINITY, NO_FLAGS));
    DEBUG_ASSERT_SC(os_connection_close(parent));
}

static int boot_log = -1;

static void boot_print (char *fmt, ...)
{
    if (boot_log == -1)
        return;

    char str[100];
    struct msg m = {
        .sys._ptr = NULL,
        .user.size = strlen(fmt),
        .user.data = fmt,
    };
    DEBUG_ASSERT_SC(os_send(boot_log, &m, NO_WAIT, NO_FLAGS));
}

static int wait_confirm_exec (int pid, int chid)
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
            if (memcmp(confirm, CONFIRM_DEV_CODE, strlen(confirm)) == 0) {
                DEBUG_ASSERT_SC(os_channel_close(pid_chid));
                return OK;
            }
        }
    }
    return ERR;
}

int main (int argc, char *argv[])
{
    if (argc >= 3) {
        char *boot_log_path = argv[2];
        boot_log = os_connection_open(boot_log_path, NO_REPLY, TIMEOUT_INFINITY);
        DEBUG_ASSERT_SC(boot_log);
    }

    boot_print("Driver manager starting ...  ");
    int input = os_channel_open(CHANNEL_PUBLIC, "dev", 4096, NO_FLAGS);
    DEBUG_ASSERT_SC(input);

        // бла бла хрень для красоты :)
        for (int i = 0; i < 5; i++) {
            os_thread_sleep(TIMEOUT_1_MS * 200);
            boot_print("\b|");
            os_thread_sleep(TIMEOUT_1_MS * 200);
            boot_print("\b/");
            os_thread_sleep(TIMEOUT_1_MS * 200);
            boot_print("\b-");
            os_thread_sleep(TIMEOUT_1_MS * 200);
            boot_print("\b\\");
            os_thread_sleep(TIMEOUT_1_MS * 200);
            boot_print("\b-");
        }
        boot_print("\b[OK]\n\r");
        os_thread_sleep(TIMEOUT_1_MS * 200);

    DEBUG_ASSERT_SC(os_connection_close(boot_log));

    if (argc >= 2) {
        char *boot_confirm_path = argv[0];
        char *boot_confirm_code = argv[1];
        send_confirm_boot(boot_confirm_path, boot_confirm_code);
    }

    int confirm_chid = os_channel_open(CHANNEL_PROTECTED, CONFIRM_DEV_PATH, 4096, CHANNEL_SINGLE_CONNECTION);
    DEBUG_ASSERT_SC(confirm_chid);

    for (;;) {
        struct connection_info info;
        DEBUG_ASSERT_SC(os_channel_wait_connection(input, &info, TIMEOUT_INFINITY));

        char *dev = info.pathname.suffix;
        if (memcmp(dev, "uart", 4) == 0) {
            char num[2];
            num[0] = *(info.pathname.suffix + 5);
            num[1] = '\0';
            if (num[0] >= '1' || num[0] <= '2') {
                struct proc_header *adr = alloc(drv_size(drv_uart));
                load(adr, drv_uart)
                spawnm(adr);
                int pid = spawnm(adr, PRIO_MAX, "dev", num, CONFIRM_DEV_PATH, CONFIRM_DEV_CODE, NULL);
                if (wait_confirm_exec(pid, confirm_chid) != OK) {
                    DEBUG_ASSERT_SC(os_channel_complete_connection(input, CONNECTION_DENY, ERR));
                    free(adr);
                    continue;
                }
                DEBUG_ASSERT_SC(os_channel_complete_connection(input, CONNECTION_CONTINUE, OK));
                continue;
            }
        }

        DEBUG_ASSERT_SC(os_channel_complete_connection(input, CONNECTION_DENY, ERR));
    }

    DEBUG_ASSERT_SC(os_channel_close(confirm_chid));

    asm volatile ("bkpt");
    return ERR;
}
