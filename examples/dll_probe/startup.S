// Test DLL entry calls an integer function and returns KErrNone.
.syntax unified
.arch armv5t
.arm
.section .text.startup, "ax", %progbits
.global _E32Startup
.type _E32Startup, %function
_E32Startup:
 .word 0xe31f0000
 cmp r4, #1
 b .Lentry
 .word 0
.Lentry:
 push {r4, lr}
 bl SymbianProbeTransform
 mov r0, #0
 pop {r4, pc}
.size _E32Startup, . - _E32Startup
.section .note.GNU-stack, "", %progbits

.section .rodata.symbian_exception, "a", %progbits
.balign 4
.global Symbian$$CPP$$Exception$$Descriptor
.type Symbian$$CPP$$Exception$$Descriptor, %object
Symbian$$CPP$$Exception$$Descriptor:
  .word __symbian_exidx_start
  .word __symbian_exidx_end
  .word __symbian_code_start
  .word __ro_end
.size Symbian$$CPP$$Exception$$Descriptor, 16
