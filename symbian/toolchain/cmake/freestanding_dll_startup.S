// SDK EKA2 entry for a DLL without runtime lifetime or owned OS resources.
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
  ldr ip, .Lentry_target
  bx ip
  .align 2
.Lentry_target:
  .word SymbianFreestandingDllEntry
.size _E32Startup, . - _E32Startup

.thumb
.thumb_func
.type SymbianFreestandingDllEntry, %function
SymbianFreestandingDllEntry:
  movs r0, #0
  bx lr
.size SymbianFreestandingDllEntry, . - SymbianFreestandingDllEntry
.section .note.GNU-stack, "", %progbits
