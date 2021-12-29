// client share mem

#include <os.h>
#include <stddef.h>

#define SERVER_CHANNEL   1

static void error ()
{
    asm volatile ("bkpt");
}

int main (int argc, char *argv[])
{
    int j = 100;
    while (j--) {
        int ch = os_channel_open(0, NULL, 4096, 0);
        if (ch < 0)
            error();
        int con = os_connection_open(SERVER_CHANNEL, NULL, 0, ch, 0);
        if (con < 0)
            error();

        mem_attributes_t attr = {
            .os_access = MEM_ACCESS_RW,
            .process_access = MEM_ACCESS_RW,
            .shared = MEM_SHARED_ON,
            .type = MEM_TYPE_NORMAL,
            .exec = MEM_EXEC_NEVER,
            .inner_cached = MEM_CACHED_WRITE_BACK,
            .outer_cached = MEM_CACHED_WRITE_BACK,
            .security = MEM_SECURITY_OFF,
            .multu_alloc = MEM_MULTU_ALLOC_OFF,
        };

        void *adr = os_mem_page_alloc(1, attr, 0);
        if (adr == NULL)
            error();

        struct sys_msg_mem sys_msg;
        sys_msg.type = SYS_MSG_SHARE_MEM;
        sys_msg.seg.adr = adr;
        sys_msg.seg.attr = attr;
        sys_msg.seg.size = 4096;

        static char fill = 0;
        fill++;

        struct msg msg_send = {0};
        msg_send.flags = 0;
        msg_send.size = sizeof(fill);
        msg_send.data = &fill;
        msg_send.sys_msg = &sys_msg;

        if (os_send(con, &msg_send, 0, 0) != OK)
            error();
        struct msg *msg_receive = NULL;
        if (os_receive(ch, &msg_receive, 0, 0) != OK)
            error();
        if (*(char*)msg_receive->data != fill)
            error();
        char *ptr = (char*)adr;
        for (int i = 4096; i; i--, ptr++) {
            if (*ptr != fill)
                error();
        }

        os_mem_page_free(adr);

        if (os_connection_close(con) < 0)
            error();
        if (os_channel_close(ch) < 0)
            error();
    }
    return 0;
}
