/*
 * Модуль синхронизации внутренних объектов ядра без вытеснения.
 * Аналог или замена linux\spinlock.h методов spin_lock_irq, spin_lock_irqsave и т.п.
 */

#ifndef KSYN_H_
#define KSYN_H_

#include <arch\types.h>
#include <arch\syn\spinlock.h>

typedef struct {
    spinlock_t slock;
    struct {
        vuint32_t state:    31;
        vuint32_t cancel:   1;
    } flags;
} kobject_lock_t;

void kobject_lock_init(kobject_lock_t *lock);
void kobject_lock_init_locked(kobject_lock_t *lock);
/**
 * Функция захватывает объект блокировки ядра lock и запрещает прерывания.
 * В SMP системе выполняет активное ожидание с выполнением попыток захвата
 * и энергосберегающим ожиданием до следующей попытки.
 *
 * Исходное состояние флага прерываний сохраняется в объекте блокировки.
 * После успешного захвата необходимо как можно скорее освободить блокировку вызовом kobject_unlock.
 * Между kobject_lock и kobject_unlock не допускается в текущем ядре вызов sched_switch.
 * @param lock
 * @return OK
 */
int kobject_lock(kobject_lock_t *lock);

/**
 * Функция разблокирует lock и восстанавливает значение флага прерываний в исходное состояние до блокировки
 * на данном объекте
 * @param lock
 */
void kobject_unlock(kobject_lock_t *lock);

/**
 * Функция разблокирует lock сохраняя запрещенные прерывания и target наследует исходное состояние
 * флага прерываний от lock (состояние до вызова kobject_lock(lock)). После этой функции
 * необходимо как можно скорее вызвать kobject_unlock(target).
 * @param lock
 * @param target
 */
void kobject_unlock_inherit (kobject_lock_t *lock, kobject_lock_t *target);

// Быстрые методы, когда гарантируется применение при запрещенных прерываниях
int kobject_qlock(kobject_lock_t *lock);
void kobject_qunlock(kobject_lock_t *lock);


#endif /* KSYN_H_ */
