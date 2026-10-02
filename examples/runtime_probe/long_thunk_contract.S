.syntax unified
.text
.arm
.p2align 2
.global RuntimeArmThunkCall
.type RuntimeArmThunkCall, %function
RuntimeArmThunkCall:
  b RuntimeReverseBytes
.size RuntimeArmThunkCall, .-RuntimeArmThunkCall
