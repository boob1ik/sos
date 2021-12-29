    .section .init, "ax"
    .arch armv7-a
    .cpu cortex-a9
    .syntax unified
    .arm
    .align 4

    .globl abort_handler_s
    .globl abort_handler

abort_handler_s:
    sub     lr, lr, #8
    stmfd   sp!, {lr}
    mrs     lr, spsr
    stmfd   sp!, {r0-r3,r12,lr}

    mov     r0, #1

    mov     r1, #0
    mrc     p15, 0, r1, c5, c0, 0   //Fault status register

    mov     r2, #0
    mrc     p15, 0, r2, c6, c0, 0   //Fault address register

    ldr     r3, [sp,#24]

    ldr     r12,=abort_handler
    blx     r12

    ldmfd   sp!, {r0-r3,r12,lr}
    msr     spsr_cxsf, lr
    ldmfd   sp!, {pc}^




    .end
