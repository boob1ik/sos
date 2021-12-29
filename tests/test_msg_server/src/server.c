// server

#include <os.h>
#include <string.h>

static void abort ()
{
    asm volatile ("bkpt");
}

static int thread (int chid)
{
    for (;;) {
        struct msg *msg_receive = NULL;
        int reply = os_receive(chid, &msg_receive, TIMEOUT_INFINITY);
        if (reply == ERR_NO_CONNECTION) {
            break;
        }
        if (reply < 0)
            abort();

        struct msg msg = {0};
        int cnt = *(int*)msg_receive->data;
        cnt++;
        msg.size = sizeof(cnt);
        msg.data = &cnt;
        msg.sys = NULL;

        if (os_send(reply, &msg, TIMEOUT_INFINITY, NO_FLAGS) != OK)
            abort();
    }
    return 0;
}

int server (int argc, char *argv[])
{
    int chid = os_channel_open(CHANNEL_PUBLIC, "server", 4096, NO_FLAGS);
    if (chid < 0)
        abort();

    for (;;) {
        struct connection_info info;
        if (os_channel_wait_connection(chid, &info, TIMEOUT_INFINITY) != OK)
            abort();

        int private_chid = os_channel_open(CHANNEL_PRIVATE, NULL, 4096, CHANNEL_AUTO_CLOSE | CHANNEL_SINGLE_CONNECTION);
        if (private_chid < 0)
            abort();
        int id = os_thread_create((void*)thread, DEFAULT_STACK, 1, (void*)private_chid);
        if (id < 0)
            abort();
        os_thread_run(id);

        if (os_channel_complete_connection(chid, CON_CMD_PRIVATE, private_chid) < 0) {
            os_channel_close(private_chid);
            os_thread_kill(id);
            abort();
        }
    }
    return -1;
}
