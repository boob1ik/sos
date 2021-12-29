#ifndef TARGET_CPU_H_
#define TARGET_CPU_H_

#include <stddef.h>
#include <stdint.h>
#include <string.h>
#include "arm_cp_registers.h"

#define MODE_USR        0x10u         // User Mode#define MODE_FIQ        0x11u         // FIQ Mode#define MODE_IRQ        0x12u         // IRQ Mode#define MODE_SVC        0x13u         // Supervisor Mode#define MODE_ABT        0x17u         // Abort Mode#define MODE_UNDEF      0x1Bu         // Undefined Mode#define MODE_SYS        0x1Fu         // System Mode#define NO_IRQ          0x80u         // when IRQ is disabled#define NO_FIQ          0x40u         // when FIQ is disabled#define NO_INTS         0xc0u         // when FIQ is disabled// FIQ нигде не используем в текущей реализации ядра

#define MODE_KERNEL_PREEMPTABLE    (MODE_SVC | NO_FIQ)
#define MODE_USER_PREEMPTABLE      (MODE_USR | NO_FIQ)

#ifdef SUPPORT_VFP
#define FLAG_VFP_ENABLED         (0x40000000u)
#endif

enum cpuid_e {
    CPU_0,
    CPU_1,
    CPU_2,
    CPU_3,

    ARCH_NUM_CORE
};

/**
 Контекст представляем в виде двух частей: постоянной и переменной.
 Контекст сохраняется в системном стеке текущего потока в режиме SVC.
 Исходный (прерванный) режим может быть как USR, так и SVC при выполнении системного вызова с вытеснением,
 поэтому в момент возникновения прерывания или выполнения команды SVC стек SVC может быть не выровнен на 8.
 По стандарту AEABI C код на C должен выполняться со стеком выровненным на 8.
 Также предпочтительно для оптимизации скорости сохранения и восстановления контекстов
 основную часть контекста располагать с выравниванием на 8 (эффективнее работает шина)

 Для этого переменная часть, которая является минимально необходимой, дополнительно
 содержит значение корректировки указателя стека SVC для выравнивания основной части контекста на 8.
 Значение корректировки равно либо 0 (основная часть контекста следует вниз сразу за переменной), либо 4
 (основная часть следует вниз через 4 неиспользуемых байта).
 Основная часть всегда имеет постоянный размер и ее регистры доступны по структуре.
 Регистры переменной части при необходимости делаются доступными через специальные методы.

 Стек убывающий, поэтому см.далее.

 О сопроцессоре VFP.
 Ядро не применяет команды VFP и не портит регистры и состояние сопроцессора. Это решается организационно
 на этапе компиляции всего проекта ядра, без непосредственной смены состояния VFP на запрещенное.
 Применяется lazy алгоритм сохранения/восстановления контекста регистров VFP пользовательских потоков.
 Это означает, что пользовательский поток не имеет доступа к регистрам и командам VFP, при
 первой команде произойдет исключение, по которому ядро включит поддержку VFP для заданного потока,
 очистит все регистры VFP и вернет ему управление в команду, которая вызвала исключение.
 С этого момента контекст данного потока становится расширенным (определяется флагом в регистре fpexc)
 и при каждом переключении будет происходить полное сохранение и восстановление регистров VFP.
 Следует отметить, что если исходный поток применял VFP, а целевой поток на который произойдет переключение
 еще не применял его, то регистры VFP будут сохранены в контекст исходного потока но не будут обнулены.
 Целевой поток (в том числе возможно другого процесса) все равно не сможет получить доступ к данным VFP
 пока ему не разрешит ядро в момент исключения. Безопасность в этом смысле решена очисткой при исключении или
 полной загрузкой регистров VFP при переключении.

 */

enum cpu_ctx_basic_regs {
    CPU_REG_0,
    CPU_REG_1,
    CPU_REG_2,
    CPU_REG_3,
    CPU_REG_4,
    CPU_REG_5,
    CPU_REG_6,
    CPU_REG_7,
    CPU_REG_8,
    CPU_REG_9,
    CPU_REG_10,
    CPU_REG_SP_USR,
    CPU_REG_LR_USR,
    CPU_REG_11,
    CPU_REG_12,
    CPU_REG_LR_SVC,
    CPU_REG_LR_RET,
    CPU_REG_SPSR_RET,

    NUM_CPU_BASIC_REGS
};

//struct cpu_start_context {
//    size_t first_regs[NUM_CPU_BASIC_FIRST_REGS];
//};

enum cpu_extended_regs {
    CPU_REG_FPEXC,
    CPU_REG_FPSCR,

    CPU_REG_D0,                     // 64-bit registers as 32-bit array, so:
    CPU_REG_D1 = CPU_REG_D0 + 2,
    CPU_REG_D2 = CPU_REG_D1 + 2,
    CPU_REG_D3 = CPU_REG_D2 + 2,
    CPU_REG_D4 = CPU_REG_D3 + 2,
    CPU_REG_D5 = CPU_REG_D4 + 2,
    CPU_REG_D6 = CPU_REG_D5 + 2,
    CPU_REG_D7 = CPU_REG_D6 + 2,
    CPU_REG_D8 = CPU_REG_D7 + 2,
    CPU_REG_D9 = CPU_REG_D8 + 2,
    CPU_REG_D10 = CPU_REG_D9 + 2,
    CPU_REG_D11 = CPU_REG_D10 + 2,
    CPU_REG_D12 = CPU_REG_D11 + 2,
    CPU_REG_D13 = CPU_REG_D12 + 2,
    CPU_REG_D14 = CPU_REG_D13 + 2,
    CPU_REG_D15 = CPU_REG_D14 + 2,
    CPU_REG_D16 = CPU_REG_D15 + 2,
    CPU_REG_D17 = CPU_REG_D16 + 2,
    CPU_REG_D18 = CPU_REG_D17 + 2,
    CPU_REG_D19 = CPU_REG_D18 + 2,
    CPU_REG_D20 = CPU_REG_D19 + 2,
    CPU_REG_D21 = CPU_REG_D20 + 2,
    CPU_REG_D22 = CPU_REG_D21 + 2,
    CPU_REG_D23 = CPU_REG_D22 + 2,
    CPU_REG_D24 = CPU_REG_D23 + 2,
    CPU_REG_D25 = CPU_REG_D24 + 2,
    CPU_REG_D26 = CPU_REG_D25 + 2,
    CPU_REG_D27 = CPU_REG_D26 + 2,
    CPU_REG_D28 = CPU_REG_D27 + 2,
    CPU_REG_D29 = CPU_REG_D28 + 2,
    CPU_REG_D30 = CPU_REG_D29 + 2,
    CPU_REG_D31 = CPU_REG_D30 + 2,

    NUM_CPU_EXTENDED_REGS = CPU_REG_D31 + 2
};

/** \Brief Структура контекста
 Изменяемая, только поле type является общим.
 Вытесненные контексты включают ext_regs и basic_regs
 */
struct cpu_context {
    int type;
#ifdef  SUPPORT_VFP
    size_t ext_regs[NUM_CPU_EXTENDED_REGS];
#endif
    size_t basic_regs[NUM_CPU_BASIC_REGS];
};

static inline void cpu_wait_energy_save ()
{
    asm volatile( "wfi" ::);
}

static inline void *cpu_get_stack_pointer (void *stack, size_t stack_size)
{
    return (stack + stack_size);
}

static inline __attribute__((__always_inline__))
        struct cpu_context *cpu_context_create (void *ksp, void *usp,
        enum cpu_context_type type, void *entry, void *exit)
{
    // basic_regs и ext_regs должны быть выровнены на 8
    struct cpu_context *ctx = ksp - sizeof(struct cpu_context);

    memset(ctx, 0, sizeof(struct cpu_context));
    ctx->type = type;
    ctx->basic_regs[CPU_REG_SP_USR] = (size_t) usp;
//    ctx->basic_regs[CPU_REG_LR_USR] = 0; // memset делает это
//    ctx->basic_regs[CPU_REG_LR_SVC] = 0;
    ctx->basic_regs[CPU_REG_LR_RET] = (size_t) entry;
    ctx->basic_regs[CPU_REG_LR_USR] = (size_t) exit;
    switch (type) {
    case CPU_CONTEXT_KERNEL_PREEMPTED:
        ctx->basic_regs[CPU_REG_SPSR_RET] = MODE_KERNEL_PREEMPTABLE;
        break;
    default:
    case CPU_CONTEXT_USER_PREEMPTED:
        ctx->basic_regs[CPU_REG_SPSR_RET] = MODE_USER_PREEMPTABLE;
        break;
    }

#ifdef SUPPORT_VFP
//    ctx->ext_regs[CPU_REG_FPSCR] = 0; // memset делает это
//    ctx->ext_regs[CPU_REG_FPEXC] = 0; // по умолчанию сопроцессор отключаем
#endif
    return ctx;
}

static inline  __attribute__((__always_inline__))
        struct cpu_context *cpu_context_reset (void *ksp, void *usp,
        enum cpu_context_type type, void *entry, void *exit) {
    // basic_regs и ext_regs должны быть выровнены на 8
    struct cpu_context *ctx = ksp - sizeof(struct cpu_context);

    ctx->type = type;
    ctx->basic_regs[CPU_REG_SP_USR] = (size_t) usp;
    ctx->basic_regs[CPU_REG_LR_USR] = 0;
    ctx->basic_regs[CPU_REG_LR_SVC] = 0;
    ctx->basic_regs[CPU_REG_LR_RET] = (size_t) entry;
    ctx->basic_regs[CPU_REG_LR_USR] = (size_t) exit;
    switch (type) {
    case CPU_CONTEXT_KERNEL_PREEMPTED:
        ctx->basic_regs[CPU_REG_SPSR_RET] = MODE_KERNEL_PREEMPTABLE;
        break;
    default:
    case CPU_CONTEXT_USER_PREEMPTED:
        ctx->basic_regs[CPU_REG_SPSR_RET] = MODE_USER_PREEMPTABLE;
        break;
    }

#ifdef  SUPPORT_VFP
    // cохраняем старое значение, оно должно быть в вытесненном контексте,
    // адрес которого у вершины стека ядра потока;
    // если поток использует сопроцессор, то сброс контекста
    // не должен сбрасывать флаг CPU_REG_FPEXC.enabled и другие настройки
#endif
    return ctx;
}

static inline __attribute__((__always_inline__)) void cpu_context_set_main_args (
        struct cpu_context *ctx, size_t arglen, void *argv, int argtype)
{
    ctx->basic_regs[CPU_REG_0] = arglen;
    ctx->basic_regs[CPU_REG_1] = (size_t)argv;
    ctx->basic_regs[CPU_REG_2] = (size_t)argtype;
}

static inline __attribute__((__always_inline__)) void cpu_context_set_thread_arg (
        struct cpu_context *ctx, void *arg) {
    ctx->basic_regs[CPU_REG_0] = (size_t)arg;
}

static inline __attribute__((__always_inline__)) void cpu_context_set_tls (
        struct cpu_context *ctx, size_t tls_size) {
    // указатель статических данных tls выравниваем на 16 байт
    ctx->basic_regs[CPU_REG_9] = ctx->basic_regs[CPU_REG_SP_USR] - ((tls_size + 0x10) & ~0xf);
    ctx->basic_regs[CPU_REG_SP_USR] = ctx->basic_regs[CPU_REG_9];
}

static inline __attribute__((__always_inline__)) void *cpu_context_get_tls (
        struct cpu_context *ctx) {
    return (void *)ctx->basic_regs[CPU_REG_9];
}

static inline __attribute__((__always_inline__)) void cpu_context_save (
        struct cpu_context *context)
{
#ifdef  SUPPORT_VFP

    register uint32_t fpexc __asm__ ("r0");
    register uint32_t fpscr __asm__ ("r1");
    register uint32_t fpctx __asm__ ("r2") = (uint32_t) (context->ext_regs);

    // регистры сопроцессора сохраняем только в пользовательский контекст,
    // причем делаем это только один раз при включенном сопроцессоре,
    // для чего выполняем проверку на 0 ячейки, соответствующей регистру FPEXC
    if(context->type == CPU_CONTEXT_USER_PREEMPTED) {
        asm volatile(
                "vmrs    %0,  fpexc         \n\
                tst      %0,  #0x40000000   \n\
                beq      1f                 \n\
                ldr      %1,  [%2]          \n\
                cmp      %1,  #0            \n\
                vmrseq   %1,  fpscr         \n\
                stmeq    %2!, {%0, %1}      \n\
                vstmeq   %2!, {d0-d15}      \n\
                vstmeq   %2!, {d16-d31}     \n\
                1: \n\
                ":"=r" (fpexc),"=r" (fpscr):"r" (fpctx));
    }
#endif
}

extern void cpu_context_switch_s (struct cpu_context *context);
static inline __attribute__((__always_inline__)) void cpu_context_switch (
        struct cpu_context *context)
{
#ifdef  SUPPORT_VFP

    register uint32_t fpscr __asm__ ("r0");
    register uint32_t fpexc __asm__ ("r1");
    register uint32_t fpctx __asm__ ("r2") = (uint32_t) (context->ext_regs);

    // регистры сопроцессора восстанавливаем
    // только из пользовательского контекста, в ядре они не используются
    if(context->type == CPU_CONTEXT_USER_PREEMPTED) {
        asm volatile(
                "ldmia   %2!, {%0, %1}       \n\
                vmsr     fpexc,  %0          \n\
                tst      %0,  #0x40000000    \n\
                vmsrne   fpscr,  %1          \n\
                vldmne   %2!, {d0-d15}       \n\
                vldmne   %2!, {d16-d31}      \n\
                ":"=r" (fpscr),"=r" (fpexc):"r" (fpctx));
    }
#endif
    cpu_context_switch_s(context);
}

static inline unsigned long cpu_get_core_id ()
{
    volatile unsigned long core = 0;

// Read Multiprocessor Affinity Register
    asm volatile ("mrc   p15, 0, %0, c0, c0, 5\n" : "=r"(core));

// Extract CPU ID bits
    asm volatile ("and   %0, %0, #3\n" : "=r"(core));
    return core;
}

/* Read System Control Register */
static inline uint32_t read_sctlr ()
{
    uint32_t ctl;
    asm volatile("mrc p15, 0, %[ctl], c1, c0, 0" : [ctl] "=r" (ctl));
    return ctl;
}

/* Write System Control Register */
static inline void write_sctlr (uint32_t ctl)
{
    asm volatile("mcr p15, 0, %[ctl], c1, c0, 0" : : [ctl] "r" (ctl));
    isb();
}

/* Read Auxiliary Control Register */
static inline uint32_t read_actlr ()
{
    uint32_t ctl;
    asm volatile("mrc p15, 0, %[ctl], c1, c0, 1" : [ctl] "=r" (ctl));
    return ctl;
}

/* Write Auxiliary Control Register */
static inline void write_actlr (uint32_t ctl)
{
    asm volatile("mcr p15, 0, %[ctl], c1, c0, 1" : : [ctl] "r" (ctl));
    isb();
}

static inline void cpu_enable_smp ()
{
    uint32_t tmp = 0;
    asm volatile("mrc     p15, 0, %[tmp], c1, c0, 1" : [tmp] "=r" (tmp));
    tmp |= BM_ACTLR_SMP;
    asm volatile("mcr     p15, 0, %[tmp], c1, c0, 1" : : [tmp] "r" (tmp));
}

static inline int cpu_enable_core (int core_id, void *entry) {
    return 0;
}

static inline int cpu_disable_core (int core_id) {
    return 0;
}

// ********************* Target CPU Memory Map ***************************

#endif /* CPU_H_ */
