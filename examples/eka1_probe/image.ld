ENTRY(Eka1Entry)
PHDRS { code PT_LOAD FLAGS(5); }
SECTIONS {
  . = 0x8000;
  .text : { KEEP(*(.text.startup)) *(.text .text.*) } :code
  .rodata : { *(.rodata .rodata.*) . = ALIGN(4); } :code
  /* No unwinding in this freestanding process. Clang's cantunwind-only
     metadata is deliberately excluded; EKA1 has no EHABI startup/runtime. */
  /DISCARD/ : { *(.ARM.exidx .ARM.exidx.*) }
  /* Unsupported allocated sections remain visible to the converter. */
  .data : { *(.data .data.*) } :code
  .bss : { *(.bss .bss.* COMMON) } :code
}
