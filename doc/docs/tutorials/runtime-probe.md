# C++ runtime probe

Exercises actual LLVM libc++ strings/vectors on the Symbian heap, with repeated
allocation/destruction and a separate fatal allocation control. Build and
emulator instructions, exact limitations and upstream provenance are in
[guest runtime guide](../capabilities/runtime.md).

This is a bounded experiment. Writable data/BSS, selected GOT fixups, EXE
global lifetime and 32-/64-bit integer division execute in the maintained
profiles. A bounded 32-bit C++ atomic probe now executes through the ROM
EUSER atomic operations on ARMv5T and ARMv6; it does not prove cross-thread
races or thread-safe libc++ ownership. TLS, local-static guards and a general
hosted C++20 runtime remain
separate gates.
