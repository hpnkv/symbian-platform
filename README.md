# Symbian development platform

Build and inspect reproducible ARM object experiments directly on macOS, and
preserve existing firmware/ROM material in verifiable host-side archives.
The target is Nokia 808 / Symbian Belle. This initial slice does not yet produce
loadable Symbian applications. See docs/RESEARCH.md and docs/STATUS.md.

```sh
uv sync
uv run symbian doctor
uv run symbian toolchain probe
uv run symbian build --project examples/abi_probe
uv run symbian inspect .symbian/build/abi_probe.o
uv run symbian device policy flash
```

All operation results are JSON with canonical status codes. Hardware commands
describe policy only; there is no device executor. Preservation instructions
are in recovery/README.md. Firmware and private device records stay outside Git.

The probe compiles twice in independent directories, checks ARM ELF32 EABI5
metadata through the native parser, compares bytes, and writes an object, report
and compilation database. It requires neither an SDK nor a target linker. Set
clangd's `--compile-commands-dir=.symbian/build` for the example. SDK declarations,
linking, E32 conversion, packaging, and loader verification remain pending.

```sh
cmake --preset debug
cmake --build --preset debug -j 8
ctest --preset debug
uv run pytest -q
uv run black --check symbian
uv run ruff check symbian
uv run python scripts/generate_stubs.py --check
```

CMake fetches the Abseil revision used by A11 and discovers or fetches GTest.
For offline builds with that exact Abseil checkout, export
`SYMBIAN_ABSEIL_SOURCE_DIR` before `uv sync` and CMake configuration.
Python installation compiles the pybind11 extension with scikit-build-core;
native core code compiles with exceptions disabled. The bindings release the
GIL around stateless native inspection. Future concurrency uses A11's Thread
library; current operations need no native scheduler or callbacks.
