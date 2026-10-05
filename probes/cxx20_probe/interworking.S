.syntax unified
.arch armv5t
// Independent ARM callback reached through a relocated pointer, including
// ARM/Thumb interworking rather than a direct linker veneer.
.section .text.arm_callback, "ax", %progbits
.arm
.global SymbianArmProbe
.type SymbianArmProbe, %function
SymbianArmProbe:
  add r0, r0, r0, lsl #4
  eor r0, r0, #0x800
  eor r0, r0, #8
  bx lr
.size SymbianArmProbe, . - SymbianArmProbe

.section .note.GNU-stack, "", %progbits
