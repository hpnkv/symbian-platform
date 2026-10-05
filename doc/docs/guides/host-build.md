# Build the host SDK and Python wheel

Use CMake/Ninja presets for native builds, GTest for native tests and Pytest
for Python and integration tests. Install prerequisites from the
[macOS and Linux source guide](source-prerequisites.md).

## Build and test from a checkout

```sh
export SYMBIAN_DEPS_PREFIX="$PWD/.symbian/host-deps"
scripts/bootstrap_wheel_deps.sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug
uv sync
uv run pytest -q
uv build --wheel
```

The isolated prefix supplies static OpenSSL, libusb, Boost and zlib.
Abseil, nlohmann::json and pybind11_abseil use pinned source revisions;
source-directory overrides allow prepared offline inputs. On Linux the
bootstrap also needs Perl's `IPC::Cmd` and `Time::Piece` modules.

Native libraries default to exceptions disabled. Selected Boost implementation
and pybind11 translation units explicitly enable exceptions where required.
Linux hides statically linked vendor symbols.

`build/debug/compile_commands.json` contains host commands and, when the source
SDK is prepared, the configured GUI's ARM commands. Host and guest builds use
separate target configurations. See [CLion setup](clion.md) and the
[guest runtime](../capabilities/runtime.md).

## Check an installed wheel

Install a built wheel into a fresh environment and run the audit outside the
checkout:

```sh
python -m venv /tmp/symbian-wheel-check
/tmp/symbian-wheel-check/bin/pip install /absolute/path/to/the.whl
cd /tmp
/tmp/symbian-wheel-check/bin/python -m symbian.build_support.audit \
  /absolute/path/to/the.whl
```

The audit checks Mach-O/ELF dependency closure, native imports, structured
statuses, CLI tools and packaged resources. Linux distribution wheels also
require manylinux repair and tagging; a local `linux_x86_64` or
`linux_aarch64` wheel is not automatically a manylinux wheel.

The host tooling wheel does not include firmware. Prepare the native target
SDK separately using [SDK installation](../reference/project-configuration.md).

## Install a standalone C++ host SDK

Configure the host install explicitly. The prefix contains headers, one static
archive with its private dependency closure, CMake configuration, licenses and
`symbian-native`. It does not include the ARM guest SDK or firmware.

```sh
export SYMBIAN_DEPS_PREFIX="$PWD/.symbian/host-deps"
scripts/bootstrap_wheel_deps.sh
cmake --preset release -DSYMBIAN_INSTALL_HOST_SDK=ON \
  -DCMAKE_INSTALL_PREFIX="$PWD/.symbian/host-sdk"
cmake --build --preset release --parallel 4
ctest --preset release
cmake --install build/release
uv run python scripts/check_host_sdk.py .symbian/host-sdk
```

The installed prefix can move without rebasing its CMake configuration. An
ordinary consumer needs only the host compiler, CMake and OS thread/system
libraries; it does not need separate Boost, Abseil, OpenSSL, libusb or zlib
installations. For a directory containing `main.cc` and `CMakeLists.txt`:

```cmake
cmake_minimum_required(VERSION 3.28)
project(export_reader LANGUAGES CXX)
find_package(SymbianHost CONFIG REQUIRED)
add_executable(export_reader main.cc)
target_link_libraries(export_reader PRIVATE Symbian::Host)
```

```cpp
#include <iostream>

#include "symbian/sdk/exports.h"

int main() {
  auto exports = symbian::sdk::ParseExports("EXPORTS\nExample @ 7 NONAME\n");
  if (!exports.ok()) {
    std::cerr << exports.status() << '\n';
    return 1;
  }
  std::cout << exports->front().ordinal << '\n';
  return 0;
}
```

```sh
cmake -S . -B build -G Ninja -DCMAKE_PREFIX_PATH=/absolute/host-sdk
cmake --build build
./build/export_reader
```

## Publish a native image without Python

`symbian-native` calls the same native format implementation as the Python
CLI. It converts a linked guest ELF and generates import-proxy sources:

```sh
/absolute/host-sdk/bin/symbian-native convert-exe \
  --input app.elf --uid3 0xE0000808 --output app.exe \
  --import-proxy /absolute/sdk/proxies/euser/euser.dso
/absolute/host-sdk/bin/symbian-native proxy-sources \
  --definition euser.def --symbol _ZN4User4ExitEi \
  --target-dll euser.dll --output generated-proxy
```

Declare every import proxy used by the ELF. For an import-free image, omit
`--import-proxy`; for the legacy process profile, add `--kernel eka1` and use
compatible GNU2 proxies. `convert-dll` additionally requires `--definition`
with frozen exports; EKA1 DLL publication is unsupported.

Proxy generation writes `exports.S`, `exports.map` and `proxy.ld` for the ARM
compiler/linker. The installed `SymbianPic` helper uses this native tool when
available. Conversion rejects unsupported ELF profiles, and output paths that
alias an input are rejected before publication. Inputs are limited to 64 MiB.

## Reuse the host archive across Python builds

After installing the C++ host SDK, build only the Python binding layer for each
interpreter. Select the prefix with `SYMBIAN_PREBUILT_HOST_SDK`:

```sh
CMAKE_ARGS="-DSYMBIAN_PREBUILT_HOST_SDK=$PWD/.symbian/host-sdk" \
  uv build --python 3.12 --wheel --out-dir dist
```

The prefix must match the source version, OS, architecture, C++ ABI and deployment
target. Build separate binding wheels for each Python version; the host archive
is reused. On macOS, set the same `MACOSX_DEPLOYMENT_TARGET` when building the
static dependencies, host SDK and binding wheels. Audit each installed wheel as
shown above.
