# Install a native SDK distribution

Install the Python command-line tools, then download the native SDK for your
host. The native archive contains the ARM compiler, linker, resource compiler,
CMake, Ninja, target headers and libraries, and the compiled C++ host SDK.
Application builds do not require a source checkout or a package-manager LLVM
installation.

## Select the archive

Use macOS 15 or later, or Ubuntu 24.04 or another Linux distribution with
glibc 2.39 or later. The Python wheels support older Linux hosts with glibc
2.28; the bundled native build tools have the higher requirement above.

| Host | Archive |
| --- | --- |
| macOS, Apple Silicon | `symbian-sdk-0.1.3-macos-arm64.tar.gz` |
| macOS, Intel | `symbian-sdk-0.1.3-macos-x86_64.tar.gz` |
| Linux, x86_64 | `symbian-sdk-0.1.3-linux-x86_64.tar.gz` |
| Linux, arm64 | `symbian-sdk-0.1.3-linux-aarch64.tar.gz` |

=== "macOS"

    ```sh
    sdk_asset="symbian-sdk-0.1.3-macos-$(uname -m).tar.gz"
    ```

=== "Linux"

    ```sh
    sdk_asset="symbian-sdk-0.1.3-linux-$(uname -m).tar.gz"
    ```

Download and install the selected archive in your activated Python environment:

```sh
pip install --upgrade symbian-platform
curl -fL "https://github.com/hpnkv/symbian-platform/releases/download/v0.1.3/$sdk_asset" \
  -o symbian-sdk.tar.gz
symbian sdk install ~/dev/symbian-sdk --archive symbian-sdk.tar.gz
symbian init ~/dev/hello_time --name hello_time --non-interactive
symbian app build --project ~/dev/hello_time
```

The archive installer needs `symbian-platform` 0.1.1 or later. It selects the
installed SDK for subsequent projects. The SDK stays visible as an ordinary
file tree; you can inspect its headers, tools and archives directly.

Both ARMv5T and ARMv6 EKA2 libraries are included in each host archive. Use
`--architecture armv5t` with `symbian init` to select ARMv5T. The libraries
include the C++ runtime, Abseil, concurrency, device APIs, HTTP, WebSockets and
Mbed TLS. Original OS imports require compatible system DLLs in the target
firmware. The archives do not replace firmware or provide the modern runtime
on EKA1; see [EKA1 restrictions](eka1.md).

## Build without Python

You can extract the native archive and use its tools directly. From an empty
directory, with `symbian-sdk.tar.gz` downloaded as above:

```sh
mkdir sdk
tar -xzf symbian-sdk.tar.gz -C sdk
cd sdk
bin/cmake -S examples/hello_time -B build/hello_time -G Ninja \
  -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/symbian-arm.cmake" \
  -DSYMBIAN_SDK_PREFIX="$PWD" \
  -DCMAKE_MAKE_PROGRAM="$PWD/bin/ninja"
bin/cmake --build build/hello_time
```

This builds `build/hello_time/hello_time.elf` and
`build/hello_time/e32/hello_time.exe`. Use a separate build directory and
`-DSYMBIAN_TARGET_ARCH=armv5t` to select ARMv5T. The included example is a real
GUI application with a clock, bounded time log, and Clear/Exit controls.

For your own executable target, publish E32 from CMake after linking it:

```cmake
symbian_publish_executable(my_app UID3 0xe0000830)
```

The helper uses `SYMBIAN_IMPORT_PROXIES`, or an explicit `IMPORT_PROXIES` list,
and writes the image under the build directory's `e32/` subdirectory. The
standalone `bin/symbian-native` also provides `convert-exe`, `convert-dll` and
`proxy-sources`; run it with `--help` for arguments. Packaging, signing and
emulator orchestration use the separately installed Python CLI.

## Relocate and use the C++ host SDK

The archive's `sdk.json` uses relative tool paths, so the extracted tree can be
moved. Refresh an existing project's `sdk-location.json` or select the moved
installation with `symbian app configure --project PROJECT --sdk NEW_SDK`.
The active user setting must also be updated when its selected directory moves.

The compiled host SDK lives in `host/`. Point a host CMake project's
`CMAKE_PREFIX_PATH` at that directory and use `find_package(SymbianHost CONFIG
REQUIRED)`. The [host SDK guide](host-build.md#install-a-standalone-c-host-sdk)
shows a consumer. The native archive includes its static third-party library
closure and the build tools' non-system shared libraries. Their licences are
under `licenses/` and `host/share/symbian/`.

Firmware, EKA2L1 and a guest debugger are configured separately. Continue with
[packaging and device installation](building.md), or
[firmware and emulator setup](firmware.md).
