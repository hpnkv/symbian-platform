# LLVM runtime source integration

The ignored checkout `research/upstream/llvm-project` is pinned to
`llvmorg-23.1.2`, commit `85ac560262434c9ccfc0c183ec22d4138ed647fb`.
The ordered maintained LLVM edits are `symbian-libcxx-lock-free.patch`,
`symbian-libcxx-chrono.patch`, and
`symbian-compiler-rt-armv5-softdouble.patch`, based on that exact commit. The first routes
libc++'s runtime `atomic::is_lock_free()` and
`atomic_ref::is_lock_free()` queries through the selected Symbian runtime
rather than Clang's ARM-target builtin.
Clang reports 64-bit atomics as lock-free for some ARMv6 compilations even
when this SDK deliberately links its `RFastLock`-backed 64-bit archive. The
alternate verified-native archive reports true. `atomic_ref`'s compile-time
`is_always_lock_free` stays conservatively false for 64-bit objects because
the answer varies by selected runtime. The change is guarded by
`__SYMBIAN32__` and leaves other libc++ targets unchanged. Apply it with
`git -C research/upstream/llvm-project apply ../../../research/llvm/symbian-libcxx-lock-free.patch`.
The applied checkout reverse-checks with `git -C research/upstream/llvm-project apply --reverse --check ../../../research/llvm/symbian-libcxx-lock-free.patch`.

The second patch makes original libc++ `steady_clock::now()` use the SDK's
guest monotonic-clock adapter on Symbian, while its original
`system_clock::now()` continues to use the ROM's working
`clock_gettime(CLOCK_REALTIME)`. On the RM-807 ROM,
`clock_gettime(CLOCK_MONOTONIC)` returns `EINVAL`; leaving the stock branch
would terminate the guest. The adapter reads `User::NTickCount()` with its
measured HAL period, falling back to `User::TickCount()` and its measured
period when the nanokernel period is unavailable. It extends 32-bit wrap in
process-wide atomic state. The patch changes no other target's clock path.
Its applied-state reverse check and clean-index apply check pass with:

```sh
git -C research/upstream/llvm-project apply --reverse --check ../../../research/llvm/symbian-libcxx-chrono.patch
git -C research/upstream/llvm-project apply --cached --check ../../../research/llvm/symbian-libcxx-chrono.patch
```

The third patch changes four `movw` constant loads in original compiler-rt
ARM soft-double assembly to literal loads and expands one `bfc` in the
single-precision source into two ARMv5-safe shifts. `movw`/`bfc` require
ARMv6T2, outside the SDK's ARMv5T and ARMv6 profiles. The actual arithmetic
algorithms remain LLVM's original implementations; the source and license
identity are unchanged. The clean pinned index accepts the patch and
the applied checkout reverse-checks with:

```sh
git -C research/upstream/llvm-project apply --cached --check ../../../research/llvm/symbian-compiler-rt-armv5-softdouble.patch
git -C research/upstream/llvm-project apply --reverse --check ../../../research/llvm/symbian-compiler-rt-armv5-softdouble.patch
```

`cpp/symbian/runtime/CMakeLists.txt` generates `__config_site` from LLVM's
original template and copies its original assertion-handler template. The
library and consumers use the same generated configuration, with a private
`std::__symbian` ABI namespace. It builds original `string.cpp` and
`new_helpers.cpp`; heap/ABI adapters are separate maintained source files.
This is a bounded subset, not a build of every libc++ runtime component.

Keep LLVM checkouts and generated files ignored. If further upstream changes
become necessary, add reviewed patch files here, record their base commit and
order, and test both application and reverse checks. Never conceal source
edits in an ignored checkout. Preserve Apache-2.0 WITH LLVM-exception notices and include
the original license/source obligations in eventual SDK payload distributions.
See ../../docs/RUNTIME.md for the actual build and execution gates.
