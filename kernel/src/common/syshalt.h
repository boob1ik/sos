#ifndef SYSHALT_H
#define SYSHALT_H

#include <os_types.h>

enum halt_state {
    SYSHALT_OOPS_ERROR,
    SYSHALT_KMEM_NO_MORE,
    SYSHALT_KMEM_ERROR,
    SYSHALT_MMAP_OVERLAP,
    SYSHALT_TID_LIMIT,
    SYSHALT_PID_LIMIT,
    SYSHALT_BOOT_NOT_EXIST,
    SYSHALT_THREAD_SWITCH_ERROR,
    SYSHALT_RESM_RECURSION_ERROR,
    SYSHALT_SYSCALL_ERROR, // TODO убрать

    SYSHALT_NUM
};

//! Критическая остановка системы
void syshalt (enum halt_state state);

#endif
