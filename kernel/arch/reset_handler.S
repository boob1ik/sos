    .section .init, "ax"
    .arch armv7-a
    .cpu cortex-a9
    .syntax unified
    .arm

    .globl _start
    .globl reset_handler_s

// linker script imports
    .globl __stack_fiq_end__
    .globl __stack_irq_end__
    .globl __stack_abt_end__
    .globl __stack_und_end__
    .globl __stack_svc_end__
    .globl __data_start__
    .globl __data_end__
    .globl __bss_start__
    .globl __bss_end__
    .globl _data_start_load
    .globl _iram_start
    .globl _iram_end


    .set  Mode_USR, 0x10            // User Mode
    .set  Mode_FIQ, 0x11            // FIQ Mode
    .set  Mode_IRQ, 0x12            // IRQ Mode
    .set  Mode_SVC, 0x13            // Supervisor Mode
    .set  Mode_ABT, 0x17            // Abort Mode
    .set  Mode_UNDEF, 0x1B          // Undefined Mode
    .set  Mode_SYS, 0x1F            // System Mode

    .equ  NO_IRQ, 0x80               // when IRQ is disabled
    .equ  NO_FIQ, 0x40               // when FIQ is disabled
    .equ  NO_INTS, 0xc0               // when FIQ is disabled

    .align 4
reset_handler_s:
    mrc     p15, 0, r0, c1, c0, 0       // Read CP15 System Control register
    bic     r0, r0, #(0x1 << 1)         // Clear I bit 1 - enable unaligned
    bic     r0, r0, #(0x1 << 12)        // Clear I bit 12 to disable I Cache
    bic     r0, r0, #(0x1 <<  2)        // Clear C bit  2 to disable D Cache
    bic     r0, r0, #0x1                // Clear M bit  0 to disable MMU
    bic     r0, r0, #(0x1 << 11)        // Clear Z bit 11 to disable branch prediction
    mcr     p15, 0, r0, c1, c0, 0       // Write value back to CP15 System Control register

    mrc     p15, 0, r0, c15, c0, 1      // IMX6Q chip Errata ARM: 743622
    orr     r0, r0, #0x40
    mcr     p15, 0, r0, c15, c0, 1

    mov     r0, #0
    mcr     p15, 0, r0, c8, c7, 0       // I-TLB and D-TLB invalidation
    mcr     p15, 0, r0, c7, c5, 6       // BPIALL - Invalidate entire branch predictor array

    // Common Cortex-A5,A7,A9 caches invalidation steps:
    // Invalidate L1 Instruction Cache
    mrc     p15, 1, r0, c0, c0, 1       // Read Cache Level ID Register (CLIDR)
    tst     r0, #0x3                    // Harvard Cache?
    mov     r0, #0                      // SBZ
    mcrne   p15, 0, r0, c7, c5, 0       // ICIALLU - Invalidate instruction cache and flush branch target cache

    // Invalidate Data/Unified Caches
    mrc     p15, 1, r0, c0, c0, 1       // Read CLIDR
    ands    r3, r0, #0x07000000         // Extract coherency level
    mov     r3, r3, LSR #23             // Total cache levels << 1
    beq     5f                          // If 0, no need to clean

    mov     r10, #0                     // R10 holds current cache level << 1
1:  add     r2, r10, r10, LSR #1        // R2 holds cache "Set" position
    mov     r1, r0, LSR r2              // Bottom 3 bits are the Cache-type for this level
    and     r1, r1, #7                  // Isolate those lower 3 bits
    cmp     r1, #2
    blt     4f                          // No cache or only instruction cache at this level

    mcr     p15, 2, r10, c0, c0, 0      // Write the Cache Size selection register
    isb                                 // ISB to sync the change to the CacheSizeID reg
    mrc     p15, 1, r1, c0, c0, 0       // Reads current Cache Size ID register
    and     r2, r1, #7                  // Extract the line length field
    add     r2, r2, #4                  // Add 4 for the line length offset (log2 16 bytes)
    ldr     r4, =0x3FF
    ands    r4, r4, r1, LSR #3          // R4 is the max number on the way size (right aligned)
    clz     r5, r4                      // R5 is the bit position of the way size increment
    ldr     r7, =0x7FFF
    ands    r7, r7, r1, LSR #13         // R7 is the max number of the index size (right aligned)

2:  mov     r9, r4                      // R9 working copy of the max way size (right aligned)

3:  orr     r11, r10, r9, LSL r5        // Factor in the Way number and cache number into R11
    orr     r11, r11, r7, LSL r2        // Factor in the Set number
    mcr     p15, 0, r11, c7, c6, 2      // Invalidate by Set/Way
    subs    r9, r9, #1                  // Decrement the Way number
    bge     3b
    subs    r7, r7, #1                  // Decrement the Set number
    bge     2b
4:  add     r10, r10, #2                // increment the cache number
    cmp     r3, r10
    bgt     1b

5:
    // Disable L2 cache controller
    ldr     r1, =0xa02000
    ldr     r0, =0x0
    str     r0, [r1, #0x100]

    // Invalidate L2 cache controller
    ldr     r0, =0xffff
    str     r0, [r1, #0x77c]
6:
    ldr     r0, [r1, #0x77c]
    cmp     r0, #0
    bne     6b

// Initialize Stack pointer for various CPU Modes
    msr     CPSR_c, #Mode_FIQ|NO_INTS   // No interrupts
    ldr     sp, =__stack_fiq_end__
    msr     CPSR_c, #Mode_IRQ|NO_INTS   // No interrupts
    ldr     sp, =__stack_irq_end__
    msr     CPSR_c, #Mode_ABT|NO_INTS   // No interrupts
    ldr     sp, =__stack_abt_end__
    msr     CPSR_c, #Mode_UNDEF|NO_INTS // No interrupts
    ldr     sp, =__stack_und_end__
    msr     CPSR_c, #Mode_SYS|NO_INTS   // No interrupts
    ldr     sp, =__stack_usr_end__
    msr     CPSR_c, #Mode_SVC|NO_INTS   // No interrupts
    ldr     sp, =__stack_svc_end__

// Initialize VBAR
    ldr     r0, =_start
    mcr     p15, 0, r0, c12, c0, 0  // Write CP15 Vector Base Address Register

// Initialize VFP support
    mrc     p15, 0, r0, c1, c0, 2  // CPACR -> r0
    orr     r0, r0, #0xf00000 // Enable CP10, CP11 for all accesses
                              // if FPEXC.Enabled = 1
    mcr     p15, 0, r0, c1, c0, 2  // r0 -> CPACR

    mrc     p15, 0, r0, c1, c1, 2  // NSACR -> r0
    orr     r0, #0xc00             // -//- (enable for non-secure mode)
    mcr     p15, 0, r0, c1, c1, 2  // r0 -> NSACR

// Clear the .bss section
    mov   r0,#0                 // get a zero
    ldr   r1,=__bss_start__     // point to bss start
    ldr   r2,=__bss_end__       // point to bss end
_bss_clear_loop:
    cmp   r1, r2
    strlo r0,[r1],#4
    blo   _bss_clear_loop

    ldr    pc, =main


    .end

