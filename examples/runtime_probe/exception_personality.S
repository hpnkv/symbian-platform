// Clang names the generic ARM EHABI personality __gxx_personality_v0. The
// preserved Symbian C++ ABI exports the matching three-argument personality as
// __aeabi_unwind_cpp_pr1. Tail-call the real implementation and test its catch
// and cleanup semantics in the guest; this is not a success stub.
.syntax unified
.arm
.section .text.symbian_exception_personality,"ax",%progbits
.balign 4
.global __gxx_personality_v0
.type __gxx_personality_v0,%function
__gxx_personality_v0:
  b __aeabi_unwind_cpp_pr1
.size __gxx_personality_v0, .-__gxx_personality_v0
