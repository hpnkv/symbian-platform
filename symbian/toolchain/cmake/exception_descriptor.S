.section .rodata.symbian_exception_descriptor,"a",%progbits
.balign 4
.global Symbian$$CPP$$Exception$$Descriptor
.type Symbian$$CPP$$Exception$$Descriptor,%object
Symbian$$CPP$$Exception$$Descriptor:
  .word __exidx_start
  .word __exidx_end
  // Original Symbian unwinder: bit zero selects EHABI v2 PREL31 tables.
  .word __ro_base + 1
  .word __ro_end
.size Symbian$$CPP$$Exception$$Descriptor, 16
