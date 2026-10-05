ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); }
SECTIONS {
 . = 0x8000;
  __symbian_code_start = .;
 .text : { KEEP(*(.text.startup)) *(.text*) } :code
 .rodata : { *(.rodata*) } :code
 .ARM.exidx : {
    __symbian_exidx_start = .;
    *(.ARM.exidx .ARM.exidx.*)
    __symbian_exidx_end = .;
  } :code
  .ro_tail : { LONG(0) } :code
  __ro_end = .;
 .data : { *(.data*) } :code
 .bss : { *(.bss* COMMON) } :code
}
