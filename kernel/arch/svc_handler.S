#include <config.h>

    .section .init, "ax"
    .arch armv7-a
    .cpu cortex-a9
    .syntax unified
    .arm
    .align 4

    .set  Mode_USR, 0x10            // User Mode
    .set  Mode_FIQ, 0x11            // FIQ Mode
    .set  Mode_IRQ, 0x12            // IRQ Mode
    .set  Mode_SVC, 0x13            // Supervisor Mode
    .set  Mode_ABT, 0x17            // Abort Mode
    .set  Mode_UNDEF, 0x1B          // Undefined Mode
    .set  Mode_SYS, 0x1F            // System Mode

    .set  NO_IRQ, 0x80               // when IRQ is disabled
    .set  NO_FIQ, 0x40               // when FIQ is disabled
    .set  NO_INTS, 0xc0

    .globl svc_handler_s
    .globl interrupt_handler
    .globl cpu_context_switch_s
    .globl zeroes


svc_handler_s:
    srsfd   sp!, #Mode_SVC        // USR ret lr,spsr -> SVC stack
    stmfd   sp!, {r11, r12, lr}   // формируем контекст cpu_context аналогично
                                  // обработчику irq_handler_s
                                  // структуру сохраняем для универсальности ее применения
    cpsid   i, #Mode_SYS
    mov     r11, sp         // USR/SYS sp     -> r11 -> SVC stack
    mov     r12, lr         // USR/SYS lr     -> r12 -> SVC stack
    cps     #Mode_SVC
    stmfd   sp!, {r0-r12}   // regs -> SVC stack

    sub     r0, lr, #4
    ldr     r1, [r0]
    and     r1, r1, #0xff

    ldr     r0,=zeroes
    ldmia   r0, {r3-r12}

#ifdef  SUPPORT_VFP
    sub     sp, #(256)
    stmfd   sp!, {r3, r4}   // В контексте пока пишем значения по умолчанию FPEXC = 0 и FPSCR = 0,
                            // так как не каждый восстанавливаемый контекст должен включать восстановление регистров VFP
                            // (по флагу в сохраненном FPEXC определяем, нужно ли восстанавливать регистры,
                            // необходимо исключить восстановление регистров больше одного раза перед возвратом в поток)
                            // это определяется тем какой контекст у потока был в момент его вытеснения,
                            // мы применяем метод отложенного сохранения регистров VFP по необходимости,
                            // только перед непосредственным переключением на другой поток
#endif
    stmfd   sp!, {r0}       // первым идет поле типа контекста, будет заполнено в обработчике
    mov     r0, sp          // возвращаем адрес контекста процессора struct cpu_context *ptr

    // стек SVC должен быть выровнен на 8 байт для кода на C - ARM AAPCS
    mov     r2, sp
    and     r2, r2, #4
    orr     r2, r2, #4
    sub     sp, sp, r2
    stmfd   sp!, {r2}       // SVC sp adjustement, dummy

    ldr     r2,=syscall_handler
    blx     r2
    // сюда при обычной работе не должны попадать, так как возврат из обработчика системного вызова
    // должен выполняться через обобщенный механизм переключения контекста - cpu_context_switch_s,
    // тем не менее оставим корректный выход из svc обработчика
    ldmfd   sp!, {r0}
    add     sp, sp, r0
    b       cpu_context_switch_s
