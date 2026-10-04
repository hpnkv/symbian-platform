# E32 build pipeline

This is the advanced path for an explicit CMake source graph. Start with
[building an application](../guides/building.md) if you are new to the SDK.

## Source graph and target

CMake owns source files, dependencies and `compile_commands.json`. A
`symbian.toml` project selects the CMake target and preset; source files belong
in `CMakeLists.txt`. The low-level `examples/e32_probe` illustrates an
import-free PIC executable:

```toml
[project]
name = "e32_probe"
kind = "e32-pic"
cmake_preset = "symbian-pic"
uid3 = 0xe0000808
```

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_probe LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_pic_executable(e32_probe
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES probe.cc algorithm.cc)
```

The helper applies ARMv5T/AAPCS settings, target headers, PIC, disabled
exceptions and RTTI, and the selected linker. The linker keeps relocations
for native E32 conversion. A tracked linker script defines image layout.

## Configure and verify

A `symbian-pic` CMake configure preset uses Ninja and a build directory under
`.symbian/`. The CLI supplies the installed toolchain and output tree:

```sh
uv run symbian build --project examples/e32_probe --output .symbian/e32-probe
uv run symbian inspect --format e32 .symbian/e32-probe/e32_probe.exe
```

The build publishes ELF/E32 plus a report with tool versions, inputs and logs.
It makes a second fresh build to compare output bytes on this host and toolchain.
That is a reproducibility check, not a hermetic build proof. The root
`compile_commands.json` supports editor and clangd analysis.

With the prepared research oracles, the maintained probe has an additional
specific check:

```sh
uv run symbian toolchain verify-probe .symbian/e32-probe/e32_probe.exe
```

The oracle checks this probe in a ROMless process harness. It is not a general
application test. The [runtime capability guide](../capabilities/runtime.md),
[import notes](https://github.com/hpnkv/symbian-platform/blob/main/.dev/imports.md)
and [development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
record bounded execution and remaining gates.
