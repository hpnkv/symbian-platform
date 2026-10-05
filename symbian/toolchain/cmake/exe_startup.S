// EKA2 entry contract: R4 is reason; SP points at thread-create information.
// The C++ adapter initializes SDK heap/DLLs and exits through User::Exit.
.syntax unified
.arch armv5t
.arm
.section .text.startup, "ax", %progbits
.global _E32Startup
.type _E32Startup, %function
_E32Startup:
  .word 0xe31f0000
  cmp r4, #1
  b .Lstart
  .word 0
.Lstart:
  mov r0, r4
  mov r1, sp
  bic sp, sp, #7
  bl SymbianRunThread
.Lhalt:
  b .Lhalt
.size _E32Startup, . - _E32Startup
.section .note.GNU-stack, "", %progbits
