#define SCHED_CURRENT

#include <os.h>
//#include <err.h>
//#include <stddef.h>
#include <sched.h>
#include "syscall.h"
#include <common\syshalt.h>

void (*syscall_table[SYSCALL_NUM]) (struct thread *) =
{ //
            sc_proc_create,         // SYSCALL_PROC_CREATE
            sc_proc_kill,           // SYSCALL_PROC_KILL
            sc_proc_info,           // SYSCALL_PROC_INFO

            sc_thread_create,       // SYSCALL_THREAD_CREATE
            sc_thread_exit,         // SYSCALL_THREAD_EXIT
            sc_thread_kill,         // SYSCALL_THREAD_KILL
            sc_thread_join,         // SYSCALL_THREAD_JOIN
            sc_thread_yield,        // SYSCALL_THREAD_YIELD
            sc_thread_yield_to,     // SYSCALL_THREAD_YIELD_TO
            sc_thread_run,          // SYSCALL_THREAD_RUN
            sc_thread_stop,         // SYSCALL_THREAD_STOP
            sc_thread_sleep,        // SYSCALL_THREAD_SLEEP
            sc_thread_prio,         // SYSCALL_THREAD_PRIO

            sc_mmap,                // SYSCALL_MMAP
            sc_malloc,              // SYSCALL_MALLOC
            sc_mfree,               // SYSCALL_MFREE

            sc_send,                // SYSCALL_SEND,
            sc_receive,             // SYSCALL_RECEIVE,
            sc_channel_open,        // SYSCALL_CHANNEL_OPEN,
            sc_channel_wait_connection, // SYSCALL_CHANNEL_WAIT_CONNECTION,
            sc_channel_complete_connection, // SYSCALL_CHANNEL_COMPLETE_CONNECTION,
            sc_channel_close,       // SYSCALL_CHANNEL_CLOSE,
            sc_connection_open,     // SYSCALL_CONNECTION_OPEN,
            sc_connection_close,    // SYSCALL_CONNECTION_CLOSE,

            sc_irq_hook,            // SYSCALL_IRQ_HOOK,
            sc_irq_release,         // SYSCALL_IRQ_RELEASE,
            sc_irq_ctrl,            // SYSCALL_IRQ_CTRL,

            sc_syn_create,          // SYSCALL_SYN_CREATE,
            sc_syn_delete,          // SYSCALL_SYN_DELETE,
            sc_syn_open,            // SYSCALL_SYN_OPEN,
            sc_syn_close,           // SYSCALL_SYN_CLOSE,
            sc_syn_wait,            // SYSCALL_SYN_WAIT,
            sc_syn_done,            // SYSCALL_SYN_DONE,

            NULL,// SYSCALL_SHUTDOWN,
            NULL,// SYSCALL_GET_INFO,
            sc_time,                // SYSCALL_TIME,
            NULL// SYSCALL_CTRL,
};

void syscall_handler (struct cpu_context *ctx, int num)
{
    struct thread *thr = cur_thr();
    if ((thr->uregs != NULL) || (thr->kregs != NULL)) {
        syshalt(SYSHALT_SYSCALL_ERROR);
    }
    thr->uregs = ctx;
    ctx->type = CPU_CONTEXT_USER_PREEMPTED;
    // TODO call syscall handler
    if ((num < SYSCALL_NUM) && (syscall_table[num] != NULL)) {
        syscall_table[num](thr);
    } else {
        syshalt(SYSHALT_SYSCALL_ERROR);
    }

    sched_switch(SCHED_SWITCH_NO_RETURN); // переключаемся обратно или на другой поток
}

