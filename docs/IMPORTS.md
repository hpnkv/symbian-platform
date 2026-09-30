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

## Independent development DLL experiment

The fixture DLL contains our compiled integer function and provides no EUSER
implementation or SDK service. Its source is under cpp/tests/eka2l1/dll_source.
An isolated research producer uses Nokia's original EPL image declarations and
checksums to add an ordinal-7 export/bitmap to a validated PIC image. Its linker
script asserts the fixed function location. This trusted producer is outside
the wheel; production DLL conversion remains open.

With the research oracles built as described in research/eka2l1/README.md:

```sh
cmake --build build/eka2l1 --target symbian_dll_fixture symbian_import_probe
uv run symbian build --project cpp/tests/eka2l1/dll_source \
  --output .symbian/probe-implementation
build/eka2l1/platform-tests/symbian_dll_fixture \
  .symbian/probe-implementation/probe_impl.exe .symbian/import-probe/probe.dll
ctest --test-dir build/eka2l1 -R '^symbian_import_probe$' --output-on-failure
```

Six cases cover dyncom/dynarmic, changed input and launch after failure. They
inspect the patched slot, observe the CPU at the DLL function, and check kernel
exit/resource release. Nokia's unchanged image validator and checksum source
accept both files in separate research binaries. The process uses direct
ThreadKill and supplies no SDK heap/TLS/cleanup, DLL initialization framework,
ROM/Z or Belle services.

The Python integration rebuilds the fixture and runs all six cases when
SYMBIAN_EKA2L1_ORACLES_BUILD is supplied. Other tests cover metadata corruption,
wrong veneer targets, truncation, unsupported addends and input policy. The
SIS experiment and `verify-probe` command retain their import-free scope.

Evidence is under .symbian/import-layout/verification-report.json. The EXE digest
is `8f6cbed4ca3fe010be4d73b276d3671218e9cb3c3e4fbae29284048bced5e1a4`.
Matched DLLs, complete SDK startup/cleanup, general relocation support, ordinary
application tests and physical installation remain pending.
