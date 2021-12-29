#ifndef BARRIER_H
#define BARRIER_H

#include <os_types.h>
#include <arch\syn\spinlock.h>
/**
 * Барьер по реализации очень похож на семафор, работающий на блокировку, а не разрешение.
 * Работа с ним ведется всегда через системный вызов, так как почти всегда поток
 * должен блокироваться на время, пока счетчик не обнулится.
 * Поэтому барьеры всегда находятся в памяти ядра.
 */

typedef struct {
//    enum syn_type   type;
    int             limit;          // стартовое значение
    atomic_t        *cnt;           // основной рабочий счетчик - всегда ссылка на __cnt
    spinlock_t      wait_lock;      // спинлок очереди ожидающих потоков
    struct thread   *wait_first;
    struct thread   *wait_last;
    atomic_t        __cnt;          // BARRIER_TYPE_PSHARED
    char            *name;          // имя
} barrier_t;

int barrier_init(barrier_t *sem, syn_t *s);
int barrier_wait(barrier_t *sem, struct thread *thr, uint64_t ns);
int barrier_wait_cancel(barrier_t *sem, struct thread *thr);


#endif 
