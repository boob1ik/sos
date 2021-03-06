#ifndef SCHED_H
#define SCHED_H

#include "proc.h"

//! Настройки приоритетов
enum kernel_prio {
    KPRIO_GET_CURRENT = PRIO_GET_CURRENT,
    KPRIO_MAX = 0,
    KPRIO_DEFAULT = 4,
    KPRIO_MIN = PRIO_MIN,
    KPRIO_NUM
};

/**
    Диспетчер строится на вызовах sched_switch из следующих мест в ядре:
    - таймерное прерывание с обновлением текущего системного времени и
        вызовом sched_switch для принятия решения о возможном переключении контекста процессора при вытеснении

    - обработчик системного вызова потока, который принимает решение о смене состояния потока (обычно на блокированное)
        и переключении, а также выполняет завершение системного вызова с обратным переключением на контекст потока

    В процессе выполнения обработчика системного вызова необходимо иметь возможность перевести поток в блокировку,
    переключиться на другие потоки, а затем при возврате управления потоку (разблокировке)
    продолжить работу обработчика системного вызова в точке за sched_switch.
    В других(всех остальных?) случаях необходимо при вызове sched_switch восстановить последний контекст потока,
    сбросив при этом последнюю историю стека и состояния режима ядра. Это также актуально в случае завершения системного
    вызова с возвратом значения, так как возвращаемое значение передается через восстанавливаемый контекст.

    Таким образом, отличаем 2 типа вызова sched_switch:
    SCHED_SWITCH_NO_RETURN,     - восстановление контекста с потерей текущего сеанса ядра
    SCHED_SWITCH_SAVE_AND_RET   - сохранение текущего сеанса ядра, переключение, продолжение работы за sched_switch
                                    в текущем сеансе ядра

    Функция sched_switch учитывает рабочее время текущего исполнявшегося потока, принимает решение о
    продолжении или прекращении его выполнения в соответствии с его состоянием и политикой планирования,
    выбирает новый поток для выполнения при необходимости и переключает контекст процессора на него.

    Внешний код меняет только состояние потока, обработку смены состояния и последующие действия выполняет sched_switch.
 */

enum sched_switch_mode {
    SCHED_SWITCH_NO_RETURN,
    SCHED_SWITCH_SAVE_AND_RET
};


void sched_init ();
void sched_lock ();
void sched_unlock ();
void sched_start();

void enqueue (struct thread *thr);
void dequeue (struct thread *thr);

void sched_enqueue_switch(struct thread *thr, enum sched_switch_mode mode);
void sched_switch(enum sched_switch_mode mode);

#ifdef SCHED_CURRENT
    struct thread* cur_thr ();
    struct process* cur_proc ();
#endif

#endif
