# Build and check an EKA1 process

The opt-in EKA1 profile runs a freestanding no-UI ARMv5T process on the preserved
Nokia 7610 RH-51 / Symbian OS 8.0 fixture. Both emulator CPU backends execute it
and report its integer result as the normal process exit reason. GUI starters
and native networking currently use EKA2.

From the source workspace, with Clang/LLD available:

```sh
symbian build --project probes/eka1_probe --output .symbian/eka1-build
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
On a headless Linux host, install Xvfb as described in the
[Linux guide](linux.md) and prefix each verifier command with `xvfb-run -a`.
The complete CMake example's application is:

```cpp
int main() {
  return 7610;
}
```

The SDK supplies the ARM entry, calls `main` and returns to the emulator's
existing EKA1 heap/exit bootstrap. The native converter emits the legacy
124-byte E32 header, and inspection identifies `kernel: eka1`. It rejects
writable data/BSS, pointer fixups, lifecycle and exception metadata.
The SISX builder rejects EKA1 images; legacy SIS packaging is unsupported.

## Allocate, copy and free through original EUSER

The separate `probes/eka1_import_probe/` profile supports five original
EUSER functions through their legacy GNU2 names and ordinals. The installed SDK
provides `Symbian::Eka1EUser`; the probe links that normal CMake target. No
project-owned symbol list or import proxy is needed.

For a Python-free build, add
`symbian_publish_executable(eka1_import_probe UID3 0xe0000761)` after linking
that target. The helper selects legacy EKA1 conversion from the project's
profile and writes `e32/eka1_import_probe.exe` in the build directory.

```sh
symbian build --project probes/eka1_import_probe \
  --output .symbian/eka1-import-build
symbian toolchain verify-eka1 .symbian/eka1-import-build/eka1_import_probe.exe \
  --firmware 7610 --store .symbian/firmware-store \
  --backend dynarmic --output .symbian/eka1-import-dynarmic
symbian toolchain verify-eka1 .symbian/eka1-import-build/eka1_import_probe.exe \
  --firmware 7610 --store .symbian/firmware-store \
  --backend dyncom --output .symbian/eka1-import-dyncom
```

For example, replace that profile's `probe.cc` with this complete small
application to copy stack bytes into a heap allocation and release it:

```cpp
#include "legacy_euser.h"

int main() {
  const unsigned char source[4] = {1, 2, 3, 4};
  auto* dest = static_cast<unsigned char*>(LegacyAlloc(4));
  if (dest == nullptr) {
    return -4;
  }
  const bool copied =
      LegacyCopy(dest, source, 4) == dest + 4 && dest[0] == 1 && dest[3] == 4;
  LegacyFree(dest);
  return copied ? 7610 : 41;
}
```

`Mem::Copy` returns the end pointer. The native
converter validates retained ELF calls and emits a contiguous, zero-terminated
legacy PE IAT; inspection checks each ordinal against the import section.
Do not substitute modern Belle EUSER proxies or SDK C++ class declarations.

This profile does not supply EKA1 GUI, runtime libraries, HTTP/TLS, C++ import
ABI beyond the five explicit calls, P900 execution or physical-device
compatibility.
