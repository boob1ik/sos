    .section .init, "ax"
    .arch armv7-a
    .cpu cortex-a9
    .syntax unified
    .arm
    .align 4

    .globl undef_handler_s
    .globl undef_handler

undef_handler_s:
//    bkpt
    sub     lr, lr, #2
    stmfd   sp!, {lr}
    mrs     lr, spsr
    stmfd   sp!, {r0-r3,r12,lr}
    ldr     r2, [sp, 24]            // LR
    tst     lr, #0x20   // was ARM?
    subeq   r2, #2
    str     r2, [sp, 24]

    mrc     p15, 0, r0, c5, c0, 1   // IFSR
    mov     r1, lr                  // SPSR

    ldr     r12,=undef_handler
    blx     r12

ret:
    ldmfd   sp!, {r0-r3,r12,lr}
    msr     spsr_cxsf, lr
    ldmfd   sp!, {pc}^

    .end
