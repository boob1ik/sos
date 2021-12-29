#ifndef ARCH_CPU_H
#define ARCH_CPU_H

#include <stdint.h>
#include <stdbool.h>

/** \Brief Типы контекстов процессора
 Различаем следующие сохраняемые контексты:
 - вытесненного (прерванного обработчиком прерывания) потока в режиме пользователя
 (стандартный изолированный режим работы для всех потоков),
 - вытесненного (прерванного обработчиком прерывания) кода ядра ОС, - обработчика системного вызова из текущего потока,
 обработчик выполняется в защищенном контексте и стеке, привязанном
 к текущему исполняемому потоку (можно говорить о продолжении исполнения потока в режиме ядра в смысле учета
 времени работы и возможностей по вытеснению)
 - текущий контекст режима ядра, привязанный к текущему потоку (он сохраняется в защищенной от вытеснения секции
 обработчика системного вызова при изменении состояния потока, которое требует переключения на другой поток
 и возможного обратного возврата в обработчик для продолжения работы)
 */
enum cpu_context_type {
    CPU_CONTEXT_USER_PREEMPTED,
    CPU_CONTEXT_KERNEL_PREEMPTED,
    CPU_CONTEXT_KERNEL_SWITCH
};

struct cpu_context;

/** \Brief Универсальный метод получения начального значения
 * стекового указателя независимо от типа стека
 */
static inline void *cpu_get_stack_pointer (void *stack, size_t stack_size);

/** \Brief Создание контекста процессора
 Сохраненный контекст каждого потока находится в стеке ядра потока.
 Функция резервирует и инициализирует контекст регистров процессора
 в стеке ядра потока при создании любого потока
 ksp -   указатель на вершину стека ядра потока
 usp -   указатель на вершину стека потока
         (стек доступный потоку в процессе его выполнения)
 entry - исполняемый код потока, может быть C-функцией
 */
static inline struct cpu_context *cpu_context_create (void *ksp, void *usp,
        enum cpu_context_type, void *entry, void *exit);

static inline struct cpu_context *cpu_context_reset (void *ksp, void *usp,
        enum cpu_context_type, void *entry, void *exit);

/** \Brief Установка стандартных аргументов для нового контекста первого потока
 *  (функции main напрямую если код на C и не используются инициализируемые глобальные данные
 *   или кода инициализации стандартной библиотеки C/C++ )
 */
static inline __attribute__((__always_inline__)) void cpu_context_set_main_args (
        struct cpu_context *ctx, size_t arglen, void *argv, int argtype);

/** \Brief Установка стандартных аргументов для нового контекста вложенных потоков
 */
static inline __attribute__((__always_inline__)) void cpu_context_set_thread_arg (
        struct cpu_context *ctx, void *arg);

/**
 * Установка дополнительного резервного места для статических данных потока в области вершины его стека(user)
 * Для реализации не предполагается обязательное наличие механизмов TLS пользователя в компиляторе.
 * Этот способ рассчитан только на минимальную системную поддержку быстрого доступа к
 * основным статическим переменным для потока - tid, pid, errno и т.п.
 *
 * @param ctx
 * @param tls_size
 */
static inline __attribute__((__always_inline__)) void cpu_context_set_tls (
        struct cpu_context *ctx, size_t tls_size);

/**
 * Получение указателя на область хранения статических данных потока установленной по cpu_context_set_tls
 * @param ctx
 */
static inline __attribute__((__always_inline__)) void *cpu_context_get_tls (
        struct cpu_context *ctx);

/** \Brief Сохранение контекста процессора исходного режима
 Функция вызывается до смены контекста процессора на другой поток, чтобы сохранить
 несохраненные регистры или полный контекст исходного режима, который был до входа в текущий режим ядра или
 является текущим режимом ядра в котором необходимо выполнить переключение с возвратом
 */
static inline void cpu_context_save (struct cpu_context *context);

/** \Brief Переключение контекста процессора
 Восстанавливает контекст процессора и передает управление в сохраненную в нем точку кода
 */
static inline void cpu_context_switch (struct cpu_context *context);

static inline unsigned long cpu_get_core_id ();
static inline void cpu_enable_smp ();
static inline int cpu_enable_core (int core_id, void *entry);
static inline int cpu_disable_core (int core_id);

static inline void cpu_wait_energy_save ();

#endif
