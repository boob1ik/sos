    .section .vectors, "ax"
    .arch armv7-a
    .cpu cortex-a9
    .syntax unified
    .arm

    .globl _start

    .globl reset_handler_s
    .globl undef_handler_s
    .globl svc_handler_s
    .globl prefetch_handler_s
    .globl abort_handler_s
    .globl not_used_s
    .globl irq_handler_s
    .globl fiq_handler_s

    .globl default_handler_s

_start:

  ldr     pc,  _reset
  ldr     pc, _undef
  ldr     pc, _svc
  ldr     pc, _prefetch
  ldr     pc, _abort
  ldr     pc, _not_used
  ldr     pc, _irq
  ldr     pc, _fiq

_reset:     .word reset_handler_s
_undef:     .word undef_handler_s
_svc:       .word svc_handler_s
_prefetch:  .word prefetch_handler_s
_abort:     .word abort_handler_s
_not_used:  .word not_used_s
_irq:       .word irq_handler_s
_fiq:       .word fiq_handler_s

    .end
