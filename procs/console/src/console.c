#include <os-libc.h>
PROCNAME("console");

#include <string.h>
#include <stddef.h>
#include <stdarg.h>

#define DEBUG_ASSERT_SC(x) {if (x < 0) {err_t err = (err_t)x; asm volatile ("bkpt");}}

static int conid_output = -1;

static void output (char *buf, size_t size)
{
    if (conid_output == -1)
        return;

    struct msg m = {0};
    m.sys._ptr = NULL;
    m.user.size = size;
    m.user.data = (void *)buf;
    DEBUG_ASSERT_SC(os_send(conid_output, &m, NO_WAIT, NO_FLAGS));
}

static void print (char *str)
{
    size_t size = strlen(str);
    output(str, size);
}

static void prompt ()
{
    print("\r\n");
    char *prompt = "> ";
    print(prompt);
}

static void print_version ()
{
    print("Console ver.1.0");
}

static void print_name (char *buf)
{
    print("'");
    print(buf);
    print("'");
}

static int init_dev (char *dev_console)
{
    int input = os_channel_open(CHANNEL_PRIVATE, NULL, 4096, NO_FLAGS);
    DEBUG_ASSERT_SC(input);
    conid_output = os_connection_open(dev_console, input, TIMEOUT_INFINITY);
    DEBUG_ASSERT_SC(conid_output);
    return input;
}

static void echo (char *buf, size_t size)
{
    output(buf, size);
}

static char * get_cmd (int input)
{
#define CMD_MAX_LEN 1024
    static char cmd[CMD_MAX_LEN];
    const char LF = '\n';
    const char CR = '\r';
    int cur = 0;
    struct msg *m = NULL;
    bool end = false;
    while (!end) {
        int res = os_receive(input, &m, TIMEOUT_INFINITY);
        DEBUG_ASSERT_SC(res);
        size_t size = m->user.size;
        char *buf = (char *)m->user.data;
        echo(buf, size);
        for (int i = 0; i < size; i++) {
            cmd[cur++] = buf[i];
            if (cur >= CMD_MAX_LEN) {
                print("\n\rBuffer overload\n\r");
                return NULL;
            }

            if (cur >= 2 && (cmd[cur - 1] == LF && cmd[cur - 2] == CR)) {
                end = true;
                break;
            }
        }
    }
    cmd[cur - 2] = '\0';
    return cmd;
}

static char* skip_space (char *buf)
{
    while (*buf == ' ')
        ++buf;
    return buf;
}

static void run_cmd (char *buf)
{
    if (!buf)
        return;

    buf = skip_space(buf);

    if (*buf == 'v') {
        print_version();
        return;
    }

    if (buf == "run") {
        // загрузка процессов по адресу
        // TODO
    }

    print("Unknow command ");
    print_name(buf);
}

static void exec_cmd (int input)
{
    prompt();
    run_cmd(get_cmd(input));
}

int main (int argc, char *argv[])
{
    if (argc < 1)
        return ERR_ILLEGAL_ARGS;

    int input = init_dev(argv[0]);
    print("\r\n");
    print_version();
    for (;;) {
        exec_cmd(input);
    }

    return ERR;
}
