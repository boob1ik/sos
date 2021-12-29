// client

#include <os.h>
#include <stddef.h>

static void abort ()
{
    asm volatile ("bkpt");
}

int client (int argc, char *argv[])
{
    static int cnt = 4;
    int j = 10;
    while (j--) {
        int reply = os_channel_open(CHANNEL_PRIVATE, NULL, 4096, CHANNEL_SINGLE_CONNECTION);
        if (reply < 0)
            abort();

        int server = os_connection_open("server", reply, TIMEOUT_INFINITY);
        if (server < 0)
            abort();

        struct msg msg = {0};
        msg.size = sizeof(cnt);
        msg.data = &cnt;
        msg.sys = NULL;

        if (os_send(server, &msg, TIMEOUT_INFINITY, NO_FLAGS) != OK)
            abort();

        struct msg *msg_receive = NULL;

        if (os_receive(reply, &msg_receive, TIMEOUT_INFINITY) != OK)
            abort();

        cnt = *(int*)msg_receive->data;

        if (os_connection_close(server) < 0)
            abort();
        if (os_channel_close(reply) < 0)
            abort();
    }
    return 0;
}
