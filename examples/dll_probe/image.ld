ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); }
SECTIONS {
 . = 0x8000;
 .text : { KEEP(*(.text.startup)) *(.text*) } :code
 .rodata : { *(.rodata*) } :code
 .ARM.exidx : { *(.ARM.exidx*) } :code
 .data : { *(.data*) } :code
 .bss : { *(.bss* COMMON) } :code
}
