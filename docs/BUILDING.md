# Experimental CMake projects

The E32 build path uses CMake presets, Ninja and Clang/LLD. CMake owns the source
graph and compilation database; the platform's native library converts the
linked ELF to E32. This currently supports import-free PIC executables with
one RX segment. SDK imports, writable data, constructors and matched Belle runtime validation
remain open. The one-executable package path is documented in PACKAGING.md.

Copy examples/e32_probe as a starting point. symbian.toml selects the CMake
target and preset and supplies its experimental UID:

```toml
[project]
name = "e32_probe"
kind = "e32-pic-experiment"
cmake_preset = "symbian-pic"
uid3 = 0xe0000808
```

The name must match an executable target. Declare sources, startup and the
linker script in CMakeLists.txt. The previous source/startup/linker_script TOML
fields are rejected to avoid silently ignoring a second source graph.

```cmake
cmake_minimum_required(VERSION 3.28)
project(my_probe LANGUAGES CXX ASM)

include(SymbianPic)
symbian_add_pic_executable(e32_probe
  STARTUP startup.S
  LINKER_SCRIPT image.ld
  SOURCES probe.cc algorithm.cc)
```

The helper accepts multiple source files within the project. It compiles C++20
as Thumb ARMv5T/AAPCS soft-float with PIC, exceptions and RTTI disabled, no host
headers and no hosted runtime. Startup assembly uses ARM instructions. The
toolchain invokes ld.lld directly, retains relocation records and rejects
undefined symbols. The linker script is a tracked link dependency.

CMakePresets.json must contain the selected configure preset using Ninja.
The example has a matching build preset. The CLI supplies the installed
toolchain/module location, compiler, linker and output tree, so a copied project
also works with the installed Python wheel. For a new project, a preset can be:

```json
{
  "version": 6,
  "configurePresets": [{
    "name": "symbian-pic",
    "generator": "Ninja",
    "binaryDir": "${sourceDir}/.symbian/build"
  }],
  "buildPresets": [{
    "name": "symbian-pic",
    "configurePreset": "symbian-pic"
  }]
}
```

Build the checked ELF/E32 pair:

```sh
uv run symbian build --project examples/e32_probe --output .symbian/e32-probe
```

The primary tree remains at .symbian/e32-probe/cmake. Ninja reuses its object
files and tracks local header edits. A second fresh tree is built to compare
ELF and E32 bytes before publishing the output pair. That comparison proves
repeatability on this host/toolchain; it is not a hermetic-build attestation.
Build reports use symbian.e32-pic-experiment/v2 and retain the actual CMake
compile groups, link fragments, tool versions, input hashes and both build logs.
They keep Belle loader/runtime verification false.

For an incremental ELF-only build after configuring:

```sh
cmake --build .symbian/e32-probe/cmake --target e32_probe
```

This updates the ELF in the CMake tree. Run symbian build again to convert,
check reproducibility and publish the E32. The root compile_commands.json is
copied from CMake and points to the retained tree and actual object paths:

```sh
clangd --check=examples/e32_probe/probe.cc \
  --compile-commands-dir=.symbian/e32-probe
```

With the pinned research oracles built, validate the maintained probe:

```sh
uv run symbian toolchain verify-probe .symbian/e32-probe/e32_probe.exe
```

That command checks this specific probe with the historical validator, CPU
backends and ROMless emulator process harness. It is not a general application
test command. SDK/package/runtime work must pass the remaining PLAN.md gates.
