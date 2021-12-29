/*
 * Модуль синхронизации внутренних объектов ядра без вытеснения.
 * Аналог или замена linux\spinlock.h методов spin_lock_irq, spin_lock_irqsave и т.п.
 *
 * Наша синхронизация пока простая:
 * - с поддержкой SMP если нужно,
 * - с запрещенными прерываниями,
 * - бесконечное ожидание @TODO Для объектов ядра важно быстро поработать и отпустить лок,
 * пока без защиты от зависания ядра
 */

#include <arch.h>
#include "ksyn.h"

void kobject_lock_init(kobject_lock_t *lock) {
    spinlock_init(&(lock->slock));
    lock->flags.state = 0;
    lock->flags.cancel = 0;
}

void kobject_lock_init_locked(kobject_lock_t *lock) {
    spinlock_init(&(lock->slock));
    spinlock_lock(&(lock->slock));
    lock->flags.state = 0;
    lock->flags.cancel = 0;
}

int kobject_lock (kobject_lock_t *lock)
{
    uint32_t s;

#if defined(BUILD_SMP)
    while(1) {
        s = interrupt_disable_s();
        if(spinlock_trylock(&lock->slock) == 1) {
            break;
        }
        interrupt_enable_s(s);
        if(lock->flags.cancel) {
            return ERR;
        }
        cpu_wait_energy_save();
    }
#else
    // ВАЖНО! В одноядерной системе не может и не должно быть ситуации когда один код захватил kobject_lock_t,
    // а другой повторно пытается его же захватить, так как первый захватчик работает с запрещенными прерываниями
    // пока не освободит блокировку. Поэтому не проверяем исходное состояние лока - он всегда должен быть свободен,
    // а следовательно и не запоминаем состояние, кроме флага прерывания
    s = interrupt_disable_s();
#endif
    // получили блокировку, прерывания запрещены
    lock->flags.state = s; // сохранили состояние прерываний
    return OK;
}

void kobject_unlock (kobject_lock_t *lock)
{
//#if defined(BUILD_SMP)
        spinlock_unlock(&lock->slock);
//#endif
    uint32_t tmp = lock->flags.state;
    interrupt_enable(tmp);
}

void kobject_unlock_inherit (kobject_lock_t *lock, kobject_lock_t *target)
{
    target->flags.state = lock->flags.state;
#if defined(BUILD_SMP)
        spinlock_unlock(&lock->slock);
#endif
}

int kobject_qlock (kobject_lock_t *lock)
{
    // предполагая, что прерывания запрещены, работаем только со спинлоком
#if defined(BUILD_SMP)
    spinlock_lock(&lock->slock);
#endif
    return OK;
}

void kobject_qunlock (kobject_lock_t *lock)
{
#if defined(BUILD_SMP)
    spinlock_unlock(&lock->slock);
#endif
}

