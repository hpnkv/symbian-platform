# Eager function imports

The `e32-import-experiment` project builds an executable with selected function
imports using CMake, Clang/LLD and the native E32 converter. Imports retain DLL
identities and frozen ordinals from validated ELF proxies. Both EKA2L1 CPU
backends execute the maintained example's compiled development DLL function at
ordinal 7 in a ROMless epoc10 configuration. Matched Belle remains unverified.

```sh
uv run symbian toolchain import-proxy examples/import_probe/exports.def \
  --symbol SymbianProbeTransform --target-dll probe.dll \
  --output .symbian/probe-dll
uv run symbian build --project examples/import_probe \
  --output .symbian/import-probe
uv run symbian inspect --format e32 .symbian/import-probe/import_probe.exe
```

The project declares `kind = "e32-import-experiment"` and an `import_proxies`
list in symbian.toml. Paths resolve relative to the project, may refer to external
generated proxies, and must stay outside the executable's output tree. The CMake
helper `symbian_add_import_executable` links those proxies and retains all
relocations. Compiler-discovered headers and linked proxies are hashed. The
primary compilation database stays usable. A separate tree must produce identical
ELF/E32 bytes before publishing the pair.

The `symbian.e32-import-experiment/v1` report includes native import metadata.
Build reports keep import execution and Belle verification false. Execution
evidence belongs to a separate run tied to input and binary hashes.

## Converter profile

The native converter accepts ARM EABI5 soft-float ET_EXEC, one RX PT_LOAD and
one matching PT_DYNAMIC. The example linker script places GOT/PLT and dynamic
metadata inside the code region. Writable application data, BSS, TLS,
constructors, exports and RELA remain unsupported. Ordinary LLD layouts with
multiple load segments remain outside this profile.

Each undefined global function needs a version record matching a supplied
proxy soname and target DLL name. R_ARM_JUMP_SLOT relocations must cover unique
aligned GOT slots. There are no zero ordinals, addends, data imports or guessed
decorated UID/version names. Duplicate DLL identities, including case variants,
are rejected. Two-DLL tests verify separate version identities and ordinals.

Retained ARM BL and ARMv5 Thumb BLX calls are checked against LLD's PC-relative
ARM veneers and their own slots. The converter writes an E32 ELF import block
per DLL and replaces slots with ordinals. The E32 loader patches them eagerly;
the ELF lazy resolver is not used. Canonical native inspection checks import
bounds, strings, offsets, ordinals and padding. Python owns build/report policy.

## Native frozen DLL experiment

The `e32-dll-experiment` profile converts a trusted modern PIC ELF into a DLL.
It accepts a frozen DEF in `export_definition`, resolves visible function symbols
from the retained static symbol table, and preserves sparse ordinals and ABSENT
entries. Python supplies paths and build policy; native code owns the format.
The output has a DLL UID1, library UID2, ordinal-zero count, complete export table,
full absence bitmap when needed, and text relocations for every export pointer.
Unused bitmap bits remain set. Header CRC includes the complete variable header.
No-hole tables omit the bitmap. Full bitmaps are used even when a historical
producer would select a smaller sparse bitmap representation.

```toml
[project]
name = "probe"
kind = "e32-dll-experiment"
cmake_preset = "symbian-pic"
uid3 = 0xe0000810
export_definition = "exports.def"
```

The CMake helper is `symbian_add_pic_dll`. Its ELF transport still has ET_EXEC,
one RX segment and an EKA2 ARM entry; the native converter supplies E32 DLL
identity. The DEF must remain within the project and outside output. It is hashed
with the compiler graph and optional import proxies. Two independent builds
must agree. The `symbian.e32-dll-experiment/v1` report and native inspector expose
DLL identity, header size, export addresses/absence and code relocation offsets.
Build reports retain loader/runtime verification false.

```sh
uv run symbian build --project examples/dll_probe --output .symbian/native-dll
uv run symbian inspect --format e32 .symbian/native-dll/probe.dll
cmake --build build/eka2l1 --target symbian_import_probe
ctest --test-dir build/eka2l1 -R '^symbian_import_probe$' --output-on-failure
```

Build the importer and proxy with the commands above first. Six cases cover
both CPU backends, changed input and launch after failure. They inspect the
patched import slot and mapped export table, including relocated absent slots
and the unchanged ordinal-zero count, observe the CPU at the DLL function,
and check kernel exit/resource release. The fixture contains our compiled
integer transform and supplies no EUSER implementation or SDK service. Its
function address comes from the linker, with no fixed location assertion.

Original Nokia checksum and whole-image validation pass native DLLs with
ordinals 7, 641 and 65,535, covering variable headers and relocation pages.
A separate combined import/export conversion case passes those consumers;
its executable startup is layout evidence, not runnable DLL initialization.
Data exports, writable data/BSS/TLS, constructors, SDK heap/TLS/static initialization and full target C++ runtime
remain unsupported. DLL startup is a no-resource integer experiment.
The process uses direct ThreadKill and provides no matched ROM/Z or Belle
services. `verify-probe` and the SIS experiment require import-free executables.

The earlier original-header fixture producer remains under cpp/tests/eka2l1 as
research material outside the wheel. It omitted the ordinal-zero count and
export-pointer relocations. The structural validator and EKA2L1 export lookup
accepted it, but that did not establish the public Symbian loader's relocation
contract. The native DLL replaces that fixture in maintained runtime tests;
see RESEARCH_LOG.md for the evidence correction.

New evidence is under .symbian/native-dll/verification-report.json. The EXE digest
is `8f6cbed4ca3fe010be4d73b276d3671218e9cb3c3e4fbae29284048bced5e1a4`.
Internal RX pointer relocations are described in POINTERS.md.
Matched DLLs, complete SDK startup/cleanup, data relocation support, ordinary
application tests and physical installation remain pending.
