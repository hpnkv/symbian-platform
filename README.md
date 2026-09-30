# Symbian development platform

Build reproducible ARM objects and experimental E32 executables on macOS, and
preserve existing firmware/ROM material in verifiable host-side archives.
The target is Nokia 808 / Symbian Belle. The first E32 experiment passes historical
image validation, CPU, ROMless installation and emulator process tests.
Execution in a matched
Belle environment remains unverified.
See docs/STATUS.md.

```sh
brew install openssl@3
uv sync
uv run symbian doctor
uv run symbian toolchain probe
uv run symbian build --project examples/abi_probe
uv run symbian inspect .symbian/build/abi_probe.o
uv run symbian build --project examples/e32_probe --output .symbian/e32-probe
uv run symbian inspect --format e32 .symbian/e32-probe/e32_probe.exe
uv run symbian package --project examples/e32_probe \
  --artifact .symbian/e32-probe/e32_probe.exe
uv run symbian inspect --format sis .symbian/package/probe.sis
uv run symbian device policy flash
```

All operation results are JSON with canonical status codes. Hardware commands
describe policy only; there is no device executor. Preservation instructions
are in recovery/README.md. Firmware and private device records stay outside Git.

The probe compiles twice in independent directories, checks ARM ELF32 EABI5
metadata through the native parser, compares bytes, and writes an object, report
and compilation database. It requires neither an SDK nor a target linker. Set
clangd's `--compile-commands-dir=.symbian/build` for the object example.

The E32 experiment uses CMake presets and Ninja, and additionally requires an
ARM ELF linker (`brew install lld`). It links ARM startup to Thumb C++, preserves
relocations with LLD, converts in the native core, and compares two CMake
ELF/E32 builds. The primary tree remains usable for incremental compilation;
CMake supplies the actual clangd database. See
[docs/BUILDING.md](docs/BUILDING.md) for project configuration and migration.
It supports one read-only code
segment and internal relative references. It rejects imports, absolute
relocations, writable data, TLS and constructors. Hand-written absolute addresses
or stripped relocation records cannot be proved absent: input must come from a
trusted link retaining all relocations. The startup directly exits a thread and
is limited to a process without runtime resources. This is not an SDK or verified Belle application. Reports keep Symbian loader verification false.
Use `.symbian/e32-probe` as clangd's compilation database directory.

The native EKA2L1 build and independent oracle tests are documented in
[research/eka2l1/README.md](research/eka2l1/README.md). The local patch provides
an explicit macOS instance root and fixes CLI shutdown without device images.
Matched ROM/Z assets are still needed to boot and test a target application.
After building those research dependencies, run:

```sh
uv run symbian toolchain verify-probe .symbian/e32-probe/e32_probe.exe
```

This runs 28 independent native tests, retains JSON/logs and input/binary hashes
under `.symbian/probe-check`, and emits a structured report. It checks the
maintained probe with Nokia's unchanged whole-image validator and executes
ARM startup, Thumb C++ and the exit SVC on both EKA2L1 CPU backends at two
addresses. Eight process cases additionally use EKA2L1's loader, memory model,
scheduler and kernel SVC dispatch under its epoc10 profile. They verify normal
and failure exits, repeated launches and address-space release on both backends.
No Belle ROM/Z or system services are supplied; Belle loader/runtime
verification remains false.
Inherited GTest filters and sharding cannot silently reduce the test count.

The native unsigned SISX writer and bounded inspector are described in
[docs/PACKAGING.md](docs/PACKAGING.md). With the research oracles built, run:

```sh
uv run symbian toolchain verify-package .symbian/package/probe.sis \
  --executable .symbian/e32-probe/e32_probe.exe
```

This extends the check to 35 cases. The package installs its unchanged executable
into disposable emulator filesystems; both CPU backends launch it through the
kernel. Registry reload, uninstall and reinstall are also checked. Belle and
phone installation remain unverified. No physical-device executor is exposed.

Frozen export and original SDK-header research is available through
`toolchain import-proxy`. See [docs/SDK.md](docs/SDK.md) for a repeatable
User::Exit ordinal proxy, typed original-header link probe and clangd checks.
The eager E32 import profile executes a compiled development DLL through
EKA2L1's loader and kernel. See [docs/IMPORTS.md](docs/IMPORTS.md) for build and
research-test replay. Matched Belle SDK/runtime imports remain unverified.

```sh
cmake --preset debug
cmake --build --preset debug -j 8
ctest --preset debug
uv run pytest -q
uv run black --check symbian scripts
uv run ruff check symbian scripts
uv run python scripts/generate_stubs.py --check
```

CMake fetches the Abseil revision used by A11 and discovers or fetches GTest.
For offline builds with that exact Abseil checkout, export
`SYMBIAN_ABSEIL_SOURCE_DIR` before `uv sync` and CMake configuration.
Python installation compiles the pybind11 extension with scikit-build-core;
native core code compiles with exceptions disabled. The bindings release the
GIL around stateless native conversion and inspection. Future concurrency uses
A11's Thread library. The platform core remains synchronous; the emulator
research harness uses EKA2L1's existing guest scheduler and timer lifecycle.
