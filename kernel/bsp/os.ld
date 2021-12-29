ENTRY(_start)

FIQ_STACK_SIZE = 0x0;
IRQ_STACK_SIZE = 0x1000;
ABT_STACK_SIZE = 0x1000;
UND_STACK_SIZE = 0x1000;
SVC_STACK_SIZE = 0x1000;
USR_STACK_SIZE = 0x1000;

MEMORY
{
  KERNEL_MEM (rwx)      : ORIGIN = 0x00900000, LENGTH = 256K  /* 0x900000 - 0x93ffff (64 pages) */
  EMPTY_MEM (rwx)       : ORIGIN = 0x10000000, LENGTH = 254M  /* 0x10000000 - 0x1fdfffff () */
  KHEAP_EXT_MEM (rwx)   : ORIGIN = 0x1fe00000, LENGTH = 1M    /* 0x1fe00000 - 0x1fefffff (256 pages) */
  MMU_MEM  (rwx)        : ORIGIN = 0x1ff00000, LENGTH = 1M    /* 0x1ff00000 - 0x1fffffff (256 pages) */
}

__kernel_mem_start__ = ORIGIN(KERNEL_MEM);
__kernel_mem_end__ = ORIGIN(KERNEL_MEM) + LENGTH(KERNEL_MEM);

__empty_mem_start__ = ORIGIN(EMPTY_MEM);
__empty_mem_end__ = ORIGIN(EMPTY_MEM) + LENGTH(EMPTY_MEM);

__kheap_ext_mem_start__ = ORIGIN(KHEAP_EXT_MEM);
__kheap_ext_mem_end__ = ORIGIN(KHEAP_EXT_MEM) + LENGTH(KHEAP_EXT_MEM);

__mmu_mem_start__ = ORIGIN(MMU_MEM);
__mmu_mem_end__ = ORIGIN(MMU_MEM) + LENGTH(MMU_MEM);


SECTIONS
{
  .mmutbl (NOLOAD) : ALIGN(16K)
  {
    __mmu_tbl_start__ = .;
    . += 0x4000 << 2; 
    __mmu_tbl_end__ = .;
  } >MMU_MEM AT>MMU_MEM

  .text : ALIGN(4K)
  {
    __text_start__ = .;
    *(.vectors);
    . = ALIGN(4);
    *(.init);
    . = ALIGN(4);
    *(.text)
    *(.text.*)
    *(.gnu.warning)
    *(.glue_7t) *(.glue_7) *(.vfp11_veneer)
    . = ALIGN(4K);
    __text_end__ = .;
    _etext = . ;
    PROVIDE (etext = .);
    
    __user_text_common_start = .;
    *(.user_text_common)
    . = ALIGN(4K);
    __user_text_common_end = .;
    
  } >KERNEL_MEM AT>KERNEL_MEM


  .rodata : ALIGN(4K) 
  {
    __rodata_start__ = .;
    *(.rodata)
    *(.rodata*);
    . = ALIGN(4K);
    __rodata_end__ = .; 
  } >KERNEL_MEM AT>KERNEL_MEM

/*  .ARM.exidx   : {
*        PROVIDE_HIDDEN (__exidx_start = .);
*        *(.ARM.exidx* .gnu.linkonce.armexidx.*)
*        PROVIDE_HIDDEN (__exidx_end = .);
*   } >KERNEL_MEM AT>KERNEL_MEM
*/

  .data : ALIGN(4K)
  {
    _data_start_load = LOADADDR(.data) + (ABSOLUTE(.) - ADDR(.data));
    __data_start__ = .;
    _data = .;
    *(.data)
    *(.data.*)
    . = ALIGN(4K);
    __data_end__ = .;
    _edata = .;
    PROVIDE (edata = .);
  } >KERNEL_MEM AT>KERNEL_MEM
  
  .bss (NOLOAD): ALIGN(4K)
  {
    __bss_start__ = .;
    *(.shbss)
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)    
    . = ALIGN(4K);
    __bss_end__ = .;
  } >KERNEL_MEM AT>KERNEL_MEM
  
  /* Region to contain exception handler stacks */
  .stacks (NOLOAD) : ALIGN(4K)
  {
    __stacks_start = .;

    __stack_fiq_start__ = .;
    . += FIQ_STACK_SIZE;
    . = ALIGN(8);
    __stack_fiq_end__ = .;

    __stack_irq_start__ = .;
    . += IRQ_STACK_SIZE;
    . = ALIGN(8);
    __stack_irq_end__ = .;

    __stack_abt_start__ = .;
    . += ABT_STACK_SIZE;
    . = ALIGN(8);
    __stack_abt_end__ = .;

    __stack_und_start__ = .;
    . += UND_STACK_SIZE;
    . = ALIGN(8);
    __stack_und_end__ = .;

    __stack_usr_start__ = .;
    . += USR_STACK_SIZE;
    . = ALIGN(8);
    __stack_usr_end__ = .;

    __stack_svc_start__ = .;
    . += SVC_STACK_SIZE * 5; /* NUM_CORE + 1 (startup stack)*/
    . = ALIGN(8);
    __stack_svc_end__ = .;
    
    . = ALIGN(4K);
    __stacks_end = .;
  
  } >KERNEL_MEM AT>KERNEL_MEM

  .kheap (NOLOAD) : ALIGN(4K)
  {
    __heap_start__ = .;
    . += 16K;
    . += ORIGIN(KERNEL_MEM) + LENGTH(KERNEL_MEM) - ABSOLUTE(.) - 1;
    . = ALIGN(4096);
    __heap_end__ = .;
  } > KERNEL_MEM AT > KERNEL_MEM
  
  .kheap_ext (NOLOAD) : ALIGN(4K)
  {
    __heap_ext_start__ = .;
    . += 0x100000;
    __heap_ext_end__ = .;
  } > KHEAP_EXT_MEM  AT > KHEAP_EXT_MEM
  
}

