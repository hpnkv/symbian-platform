ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); data PT_LOAD FLAGS(6); }
SECTIONS {
 . = 0x8000;
 .text : { KEEP(*(.text.startup)) *(.text*) } :code
 .rodata : { *(.rodata*) } :code
 .got : { *(.got) } :code
 . = 0x20000000;
 .data : { *(.data .data.*) . = ALIGN(4); } :data
 .bss (NOLOAD) : { *(.bss .bss.* COMMON) . = ALIGN(4); } :data
 /DISCARD/ : { *(.ARM.exidx*) *(.ARM.extab*) *(.comment) }
}
