ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
 . = 0x8000;
 .text : { KEEP(*(.text.startup)) *(.text*) } :code
 .plt : { *(.plt) } :code
 .rodata : { *(.rodata*) } :code
 .dynsym : { *(.dynsym) } :code
 .dynstr : { *(.dynstr) } :code
 .gnu.version : { *(.gnu.version) } :code
 .gnu.version_r : { *(.gnu.version_r) } :code
 .hash : { *(.hash) } :code
 .rel.plt : { *(.rel.plt) } :code
 .got.plt : { *(.got.plt) } :code
 .dynamic : { *(.dynamic) } :code :dynamic
 .data : { *(.data .data.*) } :code
 .bss : { *(.bss .bss.* COMMON) } :code
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}
