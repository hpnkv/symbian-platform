ENTRY(_E32Startup)
PHDRS { code PT_LOAD FLAGS(5); }
SECTIONS {
 . = 0x8000;
 .text : { KEEP(*(.text.startup)) } :code
 . = 0x8080;
 .function : { *(.text.probe_function) } :code
 ASSERT(ADDR(.function) == 0x8080, "Fixture function layout changed")
 .rodata : { *(.rodata*) } :code
 .ARM.exidx : { *(.ARM.exidx*) } :code
 .data : { *(.data*) } :code
 .bss : { *(.bss* COMMON) } :code
}
