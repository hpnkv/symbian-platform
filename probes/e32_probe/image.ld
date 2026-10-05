ENTRY(_E32Startup)
PHDRS {
  code PT_LOAD FLAGS(5);
}
SECTIONS {
  . = 0x8000;
  __symbian_code_start = .;
  .text : {
    KEEP(*(.text.startup))
    *(.text .text.*)
  } :code
  .rodata : { *(.rodata .rodata.*) } :code
  .ARM.exidx : {
    __symbian_exidx_start = .;
    *(.ARM.exidx .ARM.exidx.*)
    __symbian_exidx_end = .;
  } :code
  .ro_tail : { LONG(0) } :code
  __ro_end = .;
  /* Retain unsupported allocated sections so conversion rejects them. */
  .data : { *(.data .data.*) } :code
  .bss : { *(.bss .bss.* COMMON) } :code
}
