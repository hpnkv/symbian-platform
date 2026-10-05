ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); dynamic PT_DYNAMIC FLAGS(4); }
SECTIONS {
 . = 0x8000;
 __symbian_code_start = .;
 .text : { KEEP(*(.text.startup)) *(.text*) } :code
 .plt : { *(.plt) } :code
 .rodata : { KEEP(*(.rodata.symbian_exception)) *(.rodata*) } :code
 .ARM.exidx : {
   __symbian_exidx_start = .;
   *(.ARM.exidx .ARM.exidx.*)
   __symbian_exidx_end = .;
 } :code
 .ro_tail : { LONG(0) } :code
 .dynsym : { *(.dynsym) } :code
 .dynstr : { *(.dynstr) } :code
 .gnu.version : { *(.gnu.version) } :code
 .gnu.version_r : { *(.gnu.version_r) } :code
 .hash : { *(.hash) } :code
 .rel.plt : { *(.rel.plt) } :code
 .got.plt : { *(.got.plt) } :code
 .dynamic : { *(.dynamic) } :code :dynamic
 .data.rel.ro : { *(.data.rel.ro .data.rel.ro.*) } :code
 __ro_end = .;
 .data : { *(.data .data.*) } :code
 .bss : { *(.bss .bss.* COMMON) } :code
 /DISCARD/ : { *(.ARM.extab*) *(.comment) }
}
