// EKA1 callable process entry. The emulator's existing bootstrap creates the
// heap, invokes this entry and forwards r0 to the original EUSER Thread::Exit.
.syntax unified
.section .text.startup,"ax",%progbits
.arm
.global Eka1Entry
.type Eka1Entry, %function
Eka1Entry:
  push {r4, lr}
  mov r0, #0
  mov r1, #0
  bl main
  pop {r4, lr}
  bx lr
.size Eka1Entry, . - Eka1Entry
