# Build and check an EKA1 process

The opt-in EKA1 profile runs a freestanding no-UI ARMv5T process on the preserved
Nokia 7610 RH-51 / Symbian OS 8.0 fixture. Both emulator CPU backends execute it
and report its integer result as the normal process exit reason. GUI starters
and native networking currently use EKA2.

From the source workspace, with Clang/LLD available:

```sh
symbian build --project examples/eka1_probe --output .symbian/eka1-build
symbian toolchain verify-eka1 .symbian/eka1-build/eka1_probe.exe \
  --firmware 7610 --store .symbian/firmware-store \
  --backend dynarmic --output .symbian/eka1-dynarmic
symbian toolchain verify-eka1 .symbian/eka1-build/eka1_probe.exe \
  --firmware 7610 --store .symbian/firmware-store \
  --backend dyncom --output .symbian/eka1-dyncom
```

Use your imported fixture alias. Output directories must be new. The default
expected reason is 7610; a different native result fails even if the frontend
exits successfully. `--expected-reason N` selects another oracle.
The complete CMake example's application is:

```cpp
extern "C" int Eka1Main() { return 7610; }
```

Its separate ARM entry calls this function and returns to the emulator's
existing EKA1 heap/exit bootstrap. The native converter emits the legacy
124-byte E32 header, and inspection identifies `kernel: eka1`. It rejects
imports, writable data/BSS, pointer fixups, lifecycle and exception metadata.
The maintained SISX builder rejects EKA1 images: legacy SIS is a separate gate.

This profile does not supply EKA1 GUI, runtime libraries, HTTP/TLS, C++ import
ABI, P900 execution or physical-device compatibility. The source workspace's
`.dev/EKA1.md` records exact fixture/frontend digests, the independent parser
and changed-result controls, bootstrap dependencies and remaining ABI gates.
