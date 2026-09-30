ENTRY(_E32Startup)
PHDRS {
  code PT_LOAD FLAGS(5);
}
SECTIONS {
  . = 0x8000;
  .text : {
    KEEP(*(.text.startup))
    *(.text .text.*)
  } :code
  .rodata : { *(.rodata .rodata.*) } :code
  .ARM.exidx : { *(.ARM.exidx .ARM.exidx.*) } :code
  .data.rel.ro : { *(.data.rel.ro .data.rel.ro.*) } :code
  /* Retain unsupported allocated sections so conversion rejects them. */
  .data : { *(.data .data.*) } :code
  .bss : { *(.bss .bss.* COMMON) } :code
}
