ENTRY(Eka1Entry)
PHDRS { code PT_LOAD FLAGS(5); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
  . = 0x8000;
  .text : { KEEP(*(.text.startup)) *(.text .text.*) } :code
  .plt : { *(.plt) } :code
  .rodata : { *(.rodata .rodata.*) } :code
  .dynsym : { *(.dynsym) } :code
  .dynstr : { *(.dynstr) } :code
  .gnu.version : { *(.gnu.version) } :code
  .gnu.version_r : { *(.gnu.version_r) } :code
  .hash : { *(.hash) } :code
  .rel.plt : { *(.rel.plt) } :code
  .dynamic : { *(.dynamic) } :code :dynamic
  /* The converter requires the contiguous IAT at the end of RX, keeping
     validated PC-relative PLT references intact and appending a zero word. */
  .got.plt : { *(.got.plt) } :code
  .data : { *(.data .data.*) } :code
  .bss : { *(.bss .bss.* COMMON) } :code
  /DISCARD/ : { *(.ARM.exidx .ARM.exidx.*) }
}
