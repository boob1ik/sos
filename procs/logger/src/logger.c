#include <os-libc.h>
PROCNAME("logger");

#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#define PRINT_BUF    4096

#define DEBUG_ASSERT_SC(x) {if (x < 0) {err_t err = (err_t)x; asm volatile ("bkpt");}}

static void confirm_boot (char *path, char *code)
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

static int open_logger_input ()
{
    return os_channel_open(CHANNEL_PRIVATE, LOGGER_CHANNEL, PRINT_BUF, CHANNEL_AUTO_CONNECT | CHANNEL_SINGLE_CONNECTION);
}

static int connect_logger_output (char *path)
{
    return os_connection_open(path, NO_REPLY, TIMEOUT_INFINITY);
}

int main (int argc, char *argv[])
{
    if (argc < 1)
        return ERR;

    int output = connect_logger_output(argv[0]);
    DEBUG_ASSERT_SC(output);
    int input = open_logger_input();
    DEBUG_ASSERT_SC(input);

    if (argc >= 3) {
        char *boot_confirm_path = argv[1];
        char *boot_confirm_code = argv[2];
        confirm_boot(boot_confirm_path, boot_confirm_code);
    }

    for (;;) {
        struct msg *msg = NULL;
        DEBUG_ASSERT_SC(os_receive(input, &msg, TIMEOUT_INFINITY));
        os_send(output, msg, TIMEOUT_INFINITY, NO_FLAGS);
    }

    return ERR;
}
