// EKA2 DLL entry marker/reserved word; R0 is the OS module-entry reason.
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
  // An ARM-to-Thumb branch can make LLD emit an unrelocated absolute thunk.
  // Keep the target in an explicit E32 code relocation instead.
  ldr ip, .Lentry_target
  bx ip
  .align 2
.Lentry_target:
  .word SymbianRuntimeDllEntry
.size _E32Startup, . - _E32Startup
.section .note.GNU-stack, "", %progbits
