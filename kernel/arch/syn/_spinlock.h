#ifndef TARGET_SPINLOCK_H
#define TARGET_SPINLOCK_H

#include "_barrier.h"

static inline void dsb_sev(void) {
    asm volatile(" \
            dsb \n\
            sev \n": : : "memory");
}

static inline void spinlock_init (spinlock_t *lock)
{
    spinlock_unlock(lock);
}

static inline void spinlock_lock (spinlock_t *lock)
{
    unsigned long tmp;
    asm volatile (" \
            1: ldrex   %0, [%1]     \n\
               teq %0, #0           \n\
               wfene                \n\
               strexeq %0, %2, [%1] \n\
               teqeq   %0, #0       \n\
               bne 1b               \n"
            : "=&r" (tmp)
            : "r" (&lock->lock), "r" (1)
            : "cc");
    dmb();
}

static inline int spinlock_trylock(spinlock_t *lock)
{
    unsigned long tmp;

    asm volatile ("         \
    ldrex   %0, [%1]        \n\
    teq %0, #0              \n\
    strexeq %0, %2, [%1]    \n"
    : "=&r" (tmp)
    : "r" (&lock->lock), "r" (1)
    : "cc");
    if (tmp == 0) {
        dmb();
        return 1;
    } else {
        return 0;
    }
}

static inline void spinlock_unlock (spinlock_t *lock)
{
    dmb();
    asm volatile (
"   str %1, [%0]\n"
    :
    : "r" (&lock->lock), "r" (0)
    : "cc");
    dsb_sev();
}

static inline void rw_spinlock_lock_sh (rw_spinlock_t *lock)
{
}

static inline void rw_spinlock_unlock_sh (rw_spinlock_t *lock)
{
}

static inline void rw_spinlock_lock_ex (rw_spinlock_t *lock)
{
}

static inline void rw_spinlock_unlock_ex (rw_spinlock_t *lock)
{
}

static inline void rw_spinlock_init (rw_spinlock_t *lock)
{
}

#endif
