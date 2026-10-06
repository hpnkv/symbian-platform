// Retain the original compact-EHABI runtime dependencies.
.syntax unified
.arm

// Compact EHABI tables select these original personalities by index. Retain
// their real ordinal dependencies even when LLD emits no explicit call/PLT.
.section .rodata.symbian_native_leave_personalities,"a",%progbits
.balign 4
.global symbian_native_leave_personalities
.hidden symbian_native_leave_personalities
.type symbian_native_leave_personalities,%object
symbian_native_leave_personalities:
  .word __aeabi_unwind_cpp_pr0
  .word __aeabi_unwind_cpp_pr1
  .word __aeabi_unwind_cpp_pr2
.size symbian_native_leave_personalities, .-symbian_native_leave_personalities
