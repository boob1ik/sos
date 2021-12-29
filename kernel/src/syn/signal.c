#include <os.h>
#include <arch.h>
#include <sched.h>
#include <event.h>
#include <common/utils.h>
#include <common/namespace.h>
#include <ipc/connection.h>
#include <ipc/msg.h>
#include "signal.h"
#include <common/printf.h>

static void signal_channel_pathname(struct process *p, char *dst) {
    snprintf(dst, PATHNAME_MAX_LENGTH, "/proc/%li/signal", p->pid);
}

int signal_to_proc(struct thread *from, struct process *to, int num, int code, union sigval value) {
    char pathname[PATHNAME_MAX_BUF];
    sys_msg_signal_event_t sigmsg;
    msg_t msg;
    signal_channel_pathname(to, pathname);
    int conid = open_connection(from, pathname, NO_WAIT, NO_REPLY);
    if(conid > 0) {
        sigmsg.pid = from->proc->pid;
        sigmsg.tid = 0;
        sigmsg.num = num;
        sigmsg.code = code;
        sigmsg.value = value;
        sigmsg._type = SYS_MSG_SIGNAL;
        msg.sys.sigevent = &sigmsg;
        msg.size = 0;
        msg.data = NULL;
        send(from, conid, &msg, NO_WAIT, 0);
        close_connection(from->proc, conid);
    }
    return OK;
}
