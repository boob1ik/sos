#ifndef ARCH_SPINLOCK_H
#define ARCH_SPINLOCK_H

/**
 *   Стандартный базовый элемент синхронизации.
 *   Универсален для одноядерных и SMP систем, однако
 *   ориентирован прежде всего на возможность синхронизации в SMP системах
 */
#include <os_types.h>

typedef struct {
    volatile unsigned int lock;
} rw_spinlock_t;

static inline void spinlock_init (spinlock_t *lock);
static inline void spinlock_lock (spinlock_t *lock);
static inline int spinlock_trylock (spinlock_t *lock);
static inline void spinlock_unlock (spinlock_t *lock);
static inline void rw_spinlock_lock_sh (rw_spinlock_t *lock);
static inline void rw_spinlock_unlock_sh (rw_spinlock_t *lock);
static inline void rw_spinlock_lock_ex (rw_spinlock_t *lock);
static inline void rw_spinlock_unlock_ex (rw_spinlock_t *lock);
static inline void rw_spinlock_init (rw_spinlock_t *lock);

#endif
