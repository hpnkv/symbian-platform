// Research-only RM-807 executive adapter. Thumb SVC immediates are limited
// to eight bits, so this one-call boundary enters ARM state explicitly.
.syntax unified
.text
.align 2
.arm
.global SymbianRm807SecureRandom
.type SymbianRm807SecureRandom, %function
SymbianRm807SecureRandom:
    svc #0x10a
    bx lr
.size SymbianRm807SecureRandom, .-SymbianRm807SecureRandom
