# Symbian development platform

Build reproducible ARM objects and experimental E32 executables on macOS, and
preserve existing firmware/ROM material in verifiable host-side archives.
The target is Nokia 808 / Symbian Belle. Independent parsers accept the first
E32 experiment; execution in Belle remains unverified. See docs/STATUS.md.

```sh
uv sync
uv run symbian doctor
uv run symbian toolchain probe
uv run symbian build --project examples/abi_probe
uv run symbian inspect .symbian/build/abi_probe.o
uv run symbian build --project examples/e32_probe --output .symbian/e32-probe
uv run symbian inspect --format e32 .symbian/e32-probe/e32_probe.exe
uv run symbian device policy flash
```

All operation results are JSON with canonical status codes. Hardware commands
describe policy only; there is no device executor. Preservation instructions
are in recovery/README.md. Firmware and private device records stay outside Git.

The probe compiles twice in independent directories, checks ARM ELF32 EABI5
metadata through the native parser, compares bytes, and writes an object, report
and compilation database. It requires neither an SDK nor a target linker. Set
clangd's `--compile-commands-dir=.symbian/build` for the object example.

The E32 experiment additionally requires an ARM ELF linker (`brew install lld`).
It links ARM startup to Thumb C++, preserves relocations with LLD, converts in
the native core, and compares two ELF/E32 builds. It supports one read-only code
segment and internal relative references. It rejects imports, absolute
relocations, writable data, TLS and constructors. Hand-written absolute addresses
or stripped relocation records cannot be proved absent: input must come from a
trusted link retaining all relocations. The startup directly exits a thread and
is limited to a process without runtime resources. This is not an SDK, SIS package
or verified Belle application. Reports keep Symbian loader verification false.
Use `.symbian/e32-probe` as clangd's compilation database directory.

The native EKA2L1 build and independent oracle tests are documented in
[research/eka2l1/README.md](research/eka2l1/README.md). The local patch provides
an explicit macOS instance root and fixes CLI shutdown without device images.
Matched ROM/Z assets are still needed to boot and test a target application.

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
A11's Thread library; current operations need no native scheduler or callbacks.
