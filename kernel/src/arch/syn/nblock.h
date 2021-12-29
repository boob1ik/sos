#ifndef ARCH_NBLOCK_H_
#define ARCH_NBLOCK_H_
#ifdef __cplusplus
extern "C" {
#endif

/**
 * Специальный инструментарий "none-blocking" синхронизации по шаблону
 * 1 мастер-писатель и N читателей.
 * Синхронизация не блокирующая, рассчитана на переповторы обработки информации
 * читателями в случае, если мастер-писатель изменил информацию в процессе транзакции
 * читателя по работе с информацией. Писатель не блокирующийся без ожиданий.
 *
 * Ближайший аналог - linux\seqlock.h, в котором для захвата писателем используется спинлок.
 * Поэтому seqlock может применяться несколькими писателями, однако в этом его и недостаток, -
 * он сложнее и писатели могут активно ожидать друг друга,
 * его можно захватывать и освобождать только в защищенном от вытеснения контексте, а
 * atomic_state_t может меняться в любой точке
 *
 * Реализация предусматривает независимую работу писателя и читателей, то есть
 * как в SMP системе на разных ядрах, так и на одном процессоре без блокировки прерываний.
 * Объект синхронизации atomic_state_t может применяться для синхронизации любого количества
 * данных, однако метод переповторов транзакций читателями эффективен только тогда, когда
 * они тратят мало времени на обработку, а также мало времени тратит мастер-писатель.
 * Писатель должен:
 * - выделить блок, который ведет изменение защищенных объектом синхронизации данных,
 *      методами writer_begin и writer_commit
 * Каждый читатель должен:
 * - выделить блок, который ведет транзакцию чтения защищенных объектом синхронизации данных,
 *      методами reader_begin и reader_commit;
 * - обеспечить сохранение копии значения объекта синхронизации во временной переменной
 *   при вызове reader_begin и ее передачу в вызов reader_commit;
 * - проверять результат по каждой указанной функции читателя и, в случае ошибки
 *   выполнения операции синхронизации, выполнить повторную попытку всей транзакции
 *   чтения(обработки) данных
 *
 * Требования в SMP системе:
 *     должна быть обеспечена когерентность объекта синхронизации и данных в памяти между ядрами
 *
 * Объект синхронизации обеспечивает:
 *  синхронизацию с помощью замкнутого множества состояний - 2^(N - 1) состояний
 *  (определяется разрядностью N объекта синхронизации)
 *  значений счетчика состояний, то есть идентичное состояние может повториться после N
 *  транзакций записи писателем. Таким образом, алгоритм синхронизации функционирует корректно
 *  (по отношению к читателю) при отсутствии задержки в читателе во время выполнения
 *  транзакции чтения, превышающей время указанного суммарного числа транзакций мастера-писателя.
 *  Возможность корректного применения синхронизации связано с требованиями
 *  по динамике работы читателей и писателя, то есть временем выполнения их транзакций,
 *  а также возможностью незапланированных задержек и их длительностью.
 *  Рекомендуется применять с блокированными прерываниями для читателя для 100% гарантии
 *  целостности транзакций чтения, если неизвестны параметры динамики вызовов писателя
 */

#include "barrier.h"

typedef union {
    vuint32_t val;
    struct {
        vuint32_t state : 31;
        vuint32_t lock  : 1;
    };
} atomic_state_t;

static inline void atomic_state_init(atomic_state_t *obj) {
    obj->lock = 1;
    obj->state = 0;
}

static inline void writer_begin(atomic_state_t *state) {
    state->lock = 0;
#ifdef BUILD_SMP
    dmb();
#endif
}

static inline void writer_commit(atomic_state_t *state) {
    atomic_state_t s = *state;
    s.lock = 1;
    s.state++;
    state->val = s.val;
#ifdef BUILD_SMP
    dmb();
#endif
}

static inline int reader_begin(atomic_state_t *state, atomic_state_t *temp) {
    temp->val = state->val;
    if(temp->lock) return OK;
    else return ERR;
}

static inline int reader_commit(atomic_state_t *state, atomic_state_t *temp) {
    if( state->val != temp->val ) return ERR;
    return OK;
}

#ifdef __cplusplus
}
#endif


#endif /* ARCH_NBLOCK_H_ */