#ifndef ARCH_INTERRUPT_H
#define ARCH_INTERRUPT_H

#include <os_types.h>
#include "cpu.h"
#include "syn\ksyn.h"

void interrupt_init ();

typedef union {
    uint32_t val;
    struct {
        uint32_t type               :8;
        uint32_t executing          :1;
        uint32_t releasing          :1;
        uint32_t assert_type        :8;
        uint32_t src_core_id        :8; // номер ядра, в котором зафиксировано прерывание
        uint32_t : 6;
    };
} interrupt_flags_t;

// Контекст обработки прерывания содержит:
// - тип прерывания,
// - обработчик прерывания и тип обработчика (поток или функция ядра),
// - состояние обработки прерывания (включая поддержку строгого вложения)
// - ссылку на обработчик

struct interrupt_context {
    int id;    // идентификационный номер прерывания
    int nest_level; // = const 0, если нет необходимости в поддержке строгого вложения обработки
    struct interrupt_context *nest_prev; // предыдущий уровень вложения если есть
    interrupt_flags_t flags;
    uint32_t tmp;
    void *handler;    // обработчик
    kobject_lock_t lock;
};

/**
 * Общие требования к функционированию модуля аппаратных прерываний:
 * - обработчик interrupt должен быть один и являться частью ядра ОС
 * - прерывания должны быть всегда вложенными обеспечивая как минимум
 *   формирование системных прерываний диспетчера задач по таймеру и подсчета
 *   системного времени поверх других прерываний
 * - диапазон приоритетов: 0 - минимальный приоритет, 255 - максимальный приоритет
 */

//int interrupt_get_min_prio ();
//int interrupt_get_max_prio ();
/**
 * Установка текущего приоритета выполнения в среде исполнения, который
 * может быть вытеснен событием прерывания (приоритет текущего потока
 * в одноядерном процессоре или максимальный приоритет среди нескольких потоков исполняющихся на нескольких ядрах
 * в многоядерном процессоре)
 */
void interrupt_set_env_priority (int priority);
/**
 * Указанные константы предназначены для быстрого выбора ядер процессора,
 * в которых разрешается прерывание (через один вызов interrupt_config)
 */
#define INTERRUPT_ASSERT_TO_ALL_CPU     (-1) // прерывание обязательно генерируется для всех ядер
#define INTERRUPT_ASSERT_TO_ANYONE_CPU  (-2) // одно ядро получит прерывание, любое среди всех выбранных
int interrupt_hook (int id, void *obj, int type);
int interrupt_release (int id);
struct interrupt_context *interrupt_get_context (int id);

void interrupt_set_cpu (int id, int cpu_id, int enable);
void interrupt_set_priority (int id, int priority);
void interrupt_set_assert_type (int id, int type);
void interrupt_enable (int id);
void interrupt_disable (int id);

void interrupt_soft (int id, int cpu_id);

extern void interrupt_handler (struct cpu_context *ctx);
const struct interrupt_context *interrupt_handle_begin ();
int interrupt_handle_end (int id);

#endif
