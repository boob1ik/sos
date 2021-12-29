    .section .init, "ax"
    .arch armv7-a
    .cpu cortex-a9
    .syntax unified
    .arm
    .align 4

    .globl undef_handler_s
    .globl prefetch_handler_s
    .globl not_used_s
    .globl fiq_handler_s
    .globl default_handler_s


prefetch_handler_s:
    bkpt
not_used_s:
    bkpt
fiq_handler_s:
    bkpt
default_handler_s:
    bkpt

    .end
