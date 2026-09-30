// Experimental EKA2 startup, without EUSER or a C++ runtime.
// Only for a no-resource process: direct ThreadKill skips User::Exit cleanup.
// SVC 0x73 is the EKA2L1 epoc94/epoc10 ThreadKill mapping; real Belle untested.
.syntax unified
.arch armv5t
.arm
.section .text.startup, "ax", %progbits
.global _E32Startup
.type _E32Startup, %function
_E32Startup:
  .word 0xe31f0000  // EKA2 entry version marker: tst pc, #0.
  cmp r4, #1
  b .Lstart
  .word 0          // Loader-owned code segment ID, entry + 12.
.Lstart:
  bl ProbeMain
  mov r2, r0      // Exit reason from C++.
  mvn r0, #0xfe
  bic r0, r0, #0x7f00  // KCurrentThreadHandle = 0xffff8001.
  mov r1, #0      // EExitKill.
  mov r3, #0      // Null category.
  svc #0x73
.Lhalt:
  b .Lhalt        // If exit unexpectedly returns, do not run past code.
.size _E32Startup, . - _E32Startup
.section .note.GNU-stack, "", %progbits
