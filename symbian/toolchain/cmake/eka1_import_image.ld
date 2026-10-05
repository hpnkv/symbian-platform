ENTRY(Eka1Entry)
PHDRS { code PT_LOAD FLAGS(5); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
  . = 0x8000;
  .text : { KEEP(*(.text.startup)) *(.text .text.*) } :code
  .rodata : { *(.rodata .rodata.*) . = ALIGN(4); } :code
  .plt : { *(.plt) } :code
  .dynsym : { *(.dynsym) } :code
  .dynstr : { *(.dynstr) } :code
  .gnu.version : { *(.gnu.version) } :code
  .gnu.version_r : { *(.gnu.version_r) } :code
  .hash : { *(.hash) } :code
  .rel.plt : { *(.rel.plt) } :code
  .dynamic : { *(.dynamic) } :code :dynamic
  /* Legacy PE IAT must finish RX so the SDK can append its terminator. */
  .got.plt : { *(.got.plt) } :code
  /* No unwinding in this freestanding process. Clang's cantunwind-only
     metadata is deliberately excluded; EKA1 has no EHABI startup/runtime. */
  /DISCARD/ : { *(.ARM.exidx .ARM.exidx.*) }
  /* Unsupported allocated sections remain visible to the converter. */
  .data : { *(.data .data.*) } :code
  .bss : { *(.bss .bss.* COMMON) } :code
}
