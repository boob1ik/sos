#ifndef OS_ARM_H
#define OS_ARM_H

#ifdef __cplusplus
extern "C" {
#endif

#include <os_types.h>

#define DEFAULT_PAGE_SIZE   (4096u)

typedef enum syscall {
    SYSCALL_PROC_CREATE,
    SYSCALL_PROC_KILL,
    SYSCALL_PROC_INFO,

    SYSCALL_THREAD_CREATE,
    SYSCALL_THREAD_EXIT,
    SYSCALL_THREAD_KILL,
    SYSCALL_THREAD_JOIN,
    SYSCALL_THREAD_YIELD,
    SYSCALL_THREAD_YIELD_TO,
    SYSCALL_THREAD_RUN,
    SYSCALL_THREAD_STOP,
    SYSCALL_THREAD_SLEEP,
    SYSCALL_THREAD_PRIO,

    SYSCALL_MMAP,
    SYSCALL_MALLOC,
    SYSCALL_MFREE,

    SYSCALL_SEND,
    SYSCALL_RECEIVE,
    SYSCALL_CHANNEL_OPEN,
    SYSCALL_CHANNEL_WAIT_CONNECTION,
    SYSCALL_CHANNEL_COMPLETE_CONNECTION,
    SYSCALL_CHANNEL_CLOSE,
    SYSCALL_CONNECTION_OPEN,
    SYSCALL_CONNECTION_CLOSE,

    SYSCALL_IRQ_HOOK,
    SYSCALL_IRQ_RELEASE,
    SYSCALL_IRQ_CTRL,

    SYSCALL_SYN_CREATE,
    SYSCALL_SYN_DELETE,
    SYSCALL_SYN_OPEN,
    SYSCALL_SYN_CLOSE,
    SYSCALL_SYN_WAIT,
    SYSCALL_SYN_DONE,

    SYSCALL_SHUTDOWN,
    SYSCALL_GET_INFO,
    SYSCALL_TIME,
    SYSCALL_CTRL,

    SYSCALL_NUM
} syscall_t;

__syscall int os_proc_create (const struct proc_header *hdr, struct proc_attr *attrs) {
    register int ret __asm__ ("r0");
    register const struct proc_header *header_ptr __asm__ ("r0") = (hdr);
    register struct proc_attr *pattr __asm__ ("r1") = (attrs);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_PROC_CREATE),
            "r" (header_ptr), "r" (pattr));
    return ret;
}

__syscall int os_proc_kill (int pid) {
    register  int ret __asm__ ("r0");
    register  const int id __asm__ ("r0") = (pid);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_PROC_KILL),
            "r" (id));
    return ret;
}

__syscall int os_proc_info (int pid, struct proc_info *info) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (pid);
    register const struct proc_info *info_ptr __asm__ ("r1") = (info);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_PROC_INFO),
            "r" (id), "r" (info_ptr));
    return ret;
}

__syscall int os_thread_create (struct thread_attr *attributes) {
    register int ret __asm__ ("r0");
    register struct thread_attr *attrs __asm__ ("r0") = (attributes);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_CREATE),
            "r" (attrs));
    return ret;
}

__syscall int os_thread_exit (int result) {
    register int ret __asm__ ("r0");
    register const int res __asm__ ("r0") = (result);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_EXIT),
            "r" (res));
    return ret;
}

__syscall int os_thread_kill (int tid, int flags) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (tid);
    register const int f __asm__ ("r1") = (flags);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_KILL),
            "r" (id), "r" (f));
    return ret;
}

__syscall int os_thread_join (int tid) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (tid);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_JOIN),
            "r" (id));
    return ret;
}

__syscall int os_thread_yield () {
    register int ret __asm__ ("r0");
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_YIELD));
    return ret;
}

__syscall int os_thread_yield_to (int tid) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (tid);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_YIELD_TO),
            "r" (id));
    return ret;
}

__syscall int os_thread_run (int tid) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (tid);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_RUN),
            "r" (id));
    return ret;
}

__syscall int os_thread_stop (int tid) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (tid);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_STOP),
            "r" (id));
    return ret;
}

__syscall int os_thread_sleep (uint64_t timeout) {
    register int ret __asm__ ("r0");
    register const uint64_t ns __asm__ ("r0") = (timeout);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_SLEEP),
            "r" (ns));
    return ret;
}

__syscall int os_thread_prio (int tid, int val) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (val);
    register const int v __asm__ ("r1") = (val);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_THREAD_PRIO),
            "r" (id), "r" (v));
    return ret;
}

__syscall int os_mmap (size_t address, int num, mem_attributes_t flags) {
    register int ret __asm__ ("r0");
    register const size_t addr __asm__ ("r0") = (address);
    register const int n __asm__ ("r1") = (num);
    register const mem_attributes_t f __asm__ ("r2") = (flags);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_MMAP),
            "r" (addr), "r" (n), "r" (f));
    return ret;
}

__syscall void* os_malloc (int num, mem_attributes_t flags, size_t align) {
    register void *ret __asm__ ("r0");
    register const int n __asm__ ("r0") = (num);
    register const mem_attributes_t f __asm__ ("r1") = (flags);
    register const size_t a __asm__ ("r2") = (align);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_MALLOC),
            "r" (n), "r" (f), "r" (a));
    return ret;
}

__syscall int os_mfree (void *ptr) {
    register int ret __asm__ ("r0");
    register const void *p __asm__ ("r0") = (ptr);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_MFREE), "r" (p));
    return ret;
}

__syscall int os_send (int conid, struct msg *m, uint64_t timeout, int flags) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (conid);
    register const struct msg *msg __asm__ ("r1") = (m);
    register const uint64_t tout __asm__ ("r2") = (timeout);
    register const int f __asm__ ("r4") = (flags);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SEND), "r" (id), "r" (msg), "r" (tout), "r" (f));
    return ret;
}

__syscall int os_receive (int chid, struct msg **m, uint64_t timeout) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (chid);
    register struct msg **msg __asm__ ("r1") = (m);
    register const uint64_t tout __asm__ ("r2") = (timeout);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_RECEIVE), "r" (id), "r" (msg), "r" (tout));
    return ret;
}


__syscall int os_channel_open (channel_type_t type, char *pathname, int size, int flags) {
    register int ret __asm__ ("r0");
    register const channel_type_t t __asm__ ("r0") = (type);
    register const char *n __asm__ ("r1") = (pathname);
    register const int s __asm__ ("r2") = (size);
    register const int f __asm__ ("r3") = (flags);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_CHANNEL_OPEN), "r" (t), "r" (n), "r" (s), "r" (f));
    return ret;
}

__syscall int os_channel_wait_connection (int chid, struct connection_info *info, uint64_t timeout)
{
    register int ret __asm__ ("r0");
    register const int _id __asm__ ("r0") = (chid);
    register const struct connection_info *_i __asm__ ("r1") = (info);
    register const uint64_t _t __asm__ ("r2") = (timeout);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_CHANNEL_WAIT_CONNECTION), "r" (_id), "r" (_i), "r" (_t));
    return ret;
}

__syscall int os_channel_complete_connection (int chid, enum connection_cmd cmd, int result)
{
    register int ret __asm__ ("r0");
    register const int _id __asm__ ("r0") = (chid);
    register const enum connection_cmd _c __asm__ ("r1") = (cmd);
    register const int _r __asm__ ("r2") = (result);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_CHANNEL_COMPLETE_CONNECTION), "r" (_id), "r" (_c), "r" (_r));
    return ret;
}

__syscall int os_channel_close (int id) {
    register int ret __asm__ ("r0");
    register const int chid __asm__ ("r0") = (id);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_CHANNEL_CLOSE), "r" (chid));
    return ret;
}

__syscall int os_connection_open (char *pathname, int reply_chid, uint64_t timeout) {
    register int ret __asm__ ("r0");
    register const char *pname __asm__ ("r0") = (pathname);
    register const int replyid __asm__ ("r1") = (reply_chid);
    register const uint64_t tout __asm__ ("r2") = (timeout);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_CONNECTION_OPEN),
            "r" (pname), "r" (tout), "r" (replyid));
    return ret;
}

__syscall int os_connection_close (int id) {
    register int ret __asm__ ("r0");
    register const int conid __asm__ ("r0") = (id);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_CONNECTION_CLOSE), "r" (conid));
    return ret;
}

__syscall int os_irq_hook (int irq_id, int tid) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (irq_id);
    register const int thid __asm__ ("r1") = (tid);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_IRQ_HOOK),
            "r" (id), "r" (thid));
    return ret;
}

__syscall int os_irq_release (int irq_id) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (irq_id);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_IRQ_RELEASE),
            "r" (id));
    return ret;
}

__syscall int os_irq_ctrl (int irq_id, irq_ctrl_t ctrl) {
    register int ret __asm__ ("r0");
    register const int id __asm__ ("r0") = (irq_id);
    register const irq_ctrl_t ctl __asm__ ("r1") = (ctrl);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_IRQ_CTRL),
            "r" (id), "r" (ctl));
    return ret;
}

__syscall int os_syn_create (struct syn *s) {
    register int ret __asm__ ("r0");
    register const syn_t *obj __asm__ ("r0") = (s);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SYN_CREATE),
            "r" (obj));
    return ret;
}

__syscall int os_syn_delete (int id, int unlink) {
    register int ret __asm__ ("r0");
    register const int synid __asm__ ("r0") = (id);
    register const int u __asm__ ("r1") = (unlink);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SYN_DELETE),
            "r" (synid), "r" (u));
    return ret;
}

__syscall int os_syn_open (const char *pathname, enum syn_type type) {
    register int ret __asm__ ("r0");
    register const char *pname __asm__ ("r0") = (pathname);
    register const int t __asm__ ("r1") = type;
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SYN_OPEN),
            "r" (pname), "r" (t));
    return ret;
}

__syscall int os_syn_close (int id) {
    register int ret __asm__ ("r0");
    register const int synid __asm__ ("r0") = (id);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SYN_CLOSE),
            "r" (synid));
    return ret;
}

__syscall int os_syn_wait (int id, uint64_t timeout) {
    register int ret __asm__ ("r0");
    register const int synid __asm__ ("r0") = (id);
    register const uint64_t tout __asm__ ("r2") = (timeout);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SYN_WAIT),
            "r" (synid), "r" (tout));
    return ret;
}

__syscall int os_syn_done (int id) {
    register int ret __asm__ ("r0");
    register const int synid __asm__ ("r0") = (id);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SYN_DONE),
            "r" (synid));
    return ret;
}

__syscall int os_shutdown () {
    register int ret __asm__ ("r0");
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_SHUTDOWN));
    return ret;
}

__syscall int os_get_info (int type, union os_info *info) {
    register int ret __asm__ ("r0");
    register const int t __asm__ ("r0") = (type);
    register const union os_info *nfo __asm__ ("r1") = (info);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_GET_INFO),
            "r" (t), "r" (nfo));
    return ret;
}

__syscall int os_time (int clock_id, kernel_time_t *val, kernel_time_t *newval) {
    register int ret __asm__ ("r0");
    register int t __asm__ ("r0") = (clock_id);
    register const kernel_time_t *vptr __asm__ ("r1") = (val);
    register const kernel_time_t *nvptr __asm__ ("r2") = (newval);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_TIME),
            "r" (t), "r" (vptr), "r" (nvptr));
    return ret;
}

__syscall int os_ctrl (struct os_ctrl *ctrl) {
    register int ret __asm__ ("r0");
    register struct os_ctrl *ctl __asm__ ("r0") = (ctrl);
    asm volatile  ("svc %[sc]":[ret] "=r" (ret):[sc] "i" (SYSCALL_CTRL),
            "r" (ctl));
    return ret;
}

#ifdef __cplusplus
}
#endif
#endif
