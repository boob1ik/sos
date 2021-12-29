ENTRY(__entry)

MEMORY
{
    PROCMEM (rwx): ORIGIN = 0x10200000, LENGTH = 1M
}

__text_size__ = __text_end__ - __text_start__;
__rodata_size__ = __rodata_end__ - __rodata_start__;
__data_size__ = __data_end__ - __data_start__;
__bss_size__ = __bss_end__ - __bss_start__;

SECTIONS
{
  .text : ALIGN(4K)
  {
    __text_start__ = .;
    KEEP(*(.proc_header))
    KEEP(*(.proc_header.*))
    . = ALIGN(4);
    *(.text)
    *(.text.*)
    *(.gnu.warning)
    *(.glue_7t) *(.glue_7) *(.vfp11_veneer)
    . = ALIGN(4K);
    __text_end__ = .;
    _etext = . ;
    PROVIDE (etext = .);
  } >PROCMEM AT>PROCMEM

  .rodata : ALIGN(4K) 
  {
    __rodata_start__ = .;
    *(.rodata)
    *(.rodata*)
    *(.rel.plt)
    . = ALIGN(4K);
    __rodata_end__ = .; 
  } >PROCMEM AT>PROCMEM

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
  } >PROCMEM AT>PROCMEM
  
  .bss (NOLOAD): ALIGN(4K)
  {
    __bss_start__ = .;
    *(.shbss)
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)    
    . = ALIGN(4K);
    __bss_end__ = .;
  } >PROCMEM AT>PROCMEM
}
