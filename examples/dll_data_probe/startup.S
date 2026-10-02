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
 mov r0, #0
 bx lr
.size _E32Startup, . - _E32Startup
.section .note.GNU-stack, "", %progbits
