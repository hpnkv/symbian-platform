ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
 . = 0x8000;
 .text : { KEEP(*(.text.startup)) *(.text*) } :code
 .plt : { *(.plt) } :code
 .rodata : { *(.rodata*) } :code
 .init_array : { HIDDEN(__init_array_start = .); KEEP(*(SORT_BY_INIT_PRIORITY(.init_array.*))) KEEP(*(.init_array)) HIDDEN(__init_array_end = .); } :code
 .fini_array : { HIDDEN(__fini_array_start = .); KEEP(*(SORT_BY_INIT_PRIORITY(.fini_array.*))) KEEP(*(.fini_array)) HIDDEN(__fini_array_end = .); } :code
 .dynsym : { *(.dynsym) } :code
 .dynstr : { *(.dynstr) } :code
 .gnu.version : { *(.gnu.version) } :code
 .gnu.version_r : { *(.gnu.version_r) } :code
 .hash : { *(.hash) } :code
 .rel.dyn : { *(.rel.dyn) } :code
 .rel.plt : { *(.rel.plt) } :code
 .got.plt : { *(.got.plt) } :code
 .got : { *(.got) } :code
 .dynamic : { *(.dynamic) } :code :dynamic
 . = 0x20000000;
 .data : { *(.data .data.*) . = ALIGN(4); } :data
 .bss (NOLOAD) : { *(.bss .bss.* COMMON) . = ALIGN(4); } :data
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}
