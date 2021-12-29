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

    .set  CPU_CONTEXT_KERNEL_SWITCH,    0x2

    .globl call_thread_switch_s
    .globl thread_switch
    .globl ret_thread_switch_s
    .globl syshalt


    // По контекстам см. также irq_handler.S

    // Винир для дальнейшего вызова функции переключения потока
    // cpu_context_switch_s(struct thread *from, struct thread *to, int kreturn, void *sp);
    //                              r0                  r1              r2            r3
    // Цель функции - запуск исполнения основного кода переключения
    // в отдельном для каждого ядра глобальном стеке, так как переключение между потоками
    // может потребовать смены карт памяти, в ходе которой исходный системный стек станет недоступным
    // Возможны 2 варианта вызова: с возвращением или без
    // Для возвращения после обратного переключения резервируем в текущем системном стеке место
    // для контекста и сохраняем текущий контекст ядра так, чтобы вернуться сразу в точку вызова call_thread_switch_s,
    // потом меняем текущий стек на глобальный и работаем дальше, история работы в глобальном стеке не нужна
call_thread_switch_s:
    cmp     r2, #0
    mrsne   r2, CPSR
//    stmfdne sp!, {lr}   // rfe ret
//    stmfdne sp!, {r2}   // rfe ret
//    srsfdne sp!, #Mode_SVC // to current stack
    stmfdne sp!, {r0-r12, lr}
    movne   r2, #CPU_CONTEXT_KERNEL_SWITCH  // struct cpu_context.type = CPU_CONTEXT_KERNEL_SWITCH, все поля другие
    stmfdne sp!, {r2}
    movne   r2, sp          // r0 = r0, r1 = r1, r2 = sp = struct cpu_context *kctx, специальная версия cpu_context
    mov     sp, r3          // далее на глобальном всегда доступном для ядра стеке
    ldr     r4, =thread_switch
    blx     r4

    // возвращение в ядро после обратного переключения, вызывается из cpu_context_switch_s
    // разрешение прерывания режима SVC соответствует состоянию перед вызовом переключения
    // (должно, вроде как так и будет :) TODO check)
ret_thread_switch_s:
//    mov     sp, r0
    ldmfd   sp!, {r0-r12, lr}
    msr     CPSR, r2
    bx      lr

