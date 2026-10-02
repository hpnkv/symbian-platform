.syntax unified
.text
.arm
.align 2
.global SymbianFiberSwap
.type SymbianFiberSwap, %function
// void SymbianFiberSwap(uint32_t* old_sp, uint32_t next_sp)
// A11 fiber backend prerequisite: preserve all callee-saved ARM AAPCS state.
// This function does no native leave/TRAP work and remains on one OS thread.
SymbianFiberSwap:
  stmdb sp!, {r4-r11, lr}
  str sp, [r0]
  mov sp, r1
  ldmia sp!, {r4-r11, pc}
.size SymbianFiberSwap, .-SymbianFiberSwap
