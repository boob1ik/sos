#include <common/printf.h>
#include <common/log.h>
#include "mem\kmem.h"
#include "syshalt.h"
#include <stddef.h>
#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <ipc/msg.h>
#include "arch.h"

static const char msg_oops_error[] = "Упс";
static const char msg_kmem_no_more[] = "Нет свободной памяти";
static const char msg_kmem_error[] = "Нарушение целостности памяти";
static const char msg_mmap_overlap[] = "Перекрытие карт памяти";
static const char msg_threads_limit[] = "Превышен лимит числа потоков";
static const char msg_procs_limit[] = "Превышен лимит числа процессов";
static const char msg_no_boot[] = "Отсутствует загрузчик";
static const char msg_thread_switch_error[] = "Ошибка переключения потоков";
static const char msg_syscall_error[] = "Ошибка выполнения системного вызова";

static const char *syshalt_msgmap[SYSHALT_NUM] = {
        msg_oops_error,
        msg_kmem_no_more,
        msg_kmem_error,
        msg_mmap_overlap,
        msg_threads_limit,
        msg_procs_limit,
        msg_no_boot,
        msg_thread_switch_error,
        msg_syscall_error,
};

// аварийное завершение работы
void syshalt (enum halt_state state)
{
//    mmu_disable();
    if (state < SYSHALT_NUM) {
        log_fatal(syshalt_msgmap[state]);
        log_fatal("\n\r");
    } else
        log_fatal("unknown halt state\n\r");
    log_fatal("SYSTEM HALT");
#ifdef DEBUG
    asm volatile ("bkpt");
#endif
    // TODO
    while (1) ;
}
