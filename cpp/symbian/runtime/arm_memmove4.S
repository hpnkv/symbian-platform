.syntax unified
.text
.thumb
.thumb_func
.global __aeabi_memmove4
.type __aeabi_memmove4, %function
__aeabi_memmove4:
  push {lr}
  bl __aeabi_memmove
  pop {pc}
.size __aeabi_memmove4, .-__aeabi_memmove4
