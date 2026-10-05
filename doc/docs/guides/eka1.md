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
writable data/BSS, pointer fixups, lifecycle and exception metadata.
The maintained SISX builder rejects EKA1 images: legacy SIS is a separate gate.

## Allocate, copy and free through original EUSER

The separate `examples/eka1_import_probe/` profile supports five original
EUSER functions through their legacy GNU2 names and ordinals. Build its selected
proxy from the source workspace:

```sh
python - <<'PY'
from pathlib import Path
from symbian.sdk import build_import_proxy
from symbian.toolchain.host_tools import llvm_tool

compiler = llvm_tool("clang++")
symbols = ["AllocLen__4UserPCv", "AllocSize__4UserRi", "Alloc__4Useri",
           "Copy__3MemPvPCvi", "Free__4UserPv"]
build_import_proxy(
    Path("examples/eka1_import_probe/euser.def"), symbols, "euser.dll",
    Path(".symbian/eka1-euser-proxy"), str(compiler),
    str(llvm_tool("ld.lld", sibling=compiler.parent)),
)
PY
symbian build --project examples/eka1_import_probe \
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

extern "C" int Eka1Main() {
  const unsigned char source[4] = {1, 2, 3, 4};
  auto* dest = static_cast<unsigned char*>(LegacyAlloc(4));
  if (dest == nullptr) return -4;
  const bool copied = LegacyCopy(dest, source, 4) == dest + 4 &&
                      dest[0] == 1 && dest[3] == 4;
  LegacyFree(dest);
  return copied ? 7610 : 41;
}
```

`Mem::Copy` returns the end pointer. The maintained probe checks all 64 bytes,
allocation length, live heap counts and exact restoration after free. Its
changed-result and corrupted-copy controls run on both CPU backends. The native
converter validates retained ELF calls and emits a contiguous, zero-terminated
legacy PE IAT; inspection checks each ordinal against the import section.
Do not substitute modern Belle EUSER proxies or SDK C++ class declarations.

This profile does not supply EKA1 GUI, runtime libraries, HTTP/TLS, C++ import
ABI beyond the five explicit calls, P900 execution or physical-device
compatibility. The source workspace's
`.dev/EKA1.md` records exact fixture/frontend digests, the independent parser
and changed-result controls, bootstrap dependencies and remaining ABI gates.
