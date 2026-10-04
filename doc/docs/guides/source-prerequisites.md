# Prepare the toolchain and public source

Run these commands from the repository root. The host tools compile C++ and
convert ARM ELF into Symbian E32; target headers and firmware are separate
inputs. Linux preparation is **provisional**: host native builds and a Linux
wheel have passed bounded checks, while a Linux GUI emulator session and guest
debugger have not yet been validated on an interactive host.

## 1. Install host tools

=== "macOS"

    On Apple Silicon, install Apple's command-line tools if `xcrun` does not
    find Clang. Homebrew supplies CMake, Ninja, upstream LLVM/LLD and uv:

    ```sh
    xcode-select -p
    xcrun --find clang++
    brew install uv cmake ninja lld llvm googletest openssl@3
    ```

=== "Linux (provisional)"

    On Ubuntu 24.04 or a comparable distribution, install a C++ toolchain,
    CMake 3.28+, Ninja, Clang/LLD and the bootstrap prerequisites. Package
    names can vary by distribution.

    ```sh
    sudo apt update
    sudo apt install build-essential clang clang-format lld llvm cmake ninja-build \
      git curl ca-certificates perl pkg-config autoconf automake libtool \
      python3-dev
    curl -LsSf https://astral.sh/uv/install.sh | sh
    ```

    Start a new shell if the uv installer added its directory to your `PATH`.
    The [uv installer](https://docs.astral.sh/uv/getting-started/installation/)
    also supports a pinned-version URL.

## 2. Prepare the host build and check ARM output

Use an isolated prefix for static OpenSSL, libusb and Boost, following the same
native/wheel separation as [A11](https://github.com/hpnkv/a11). The bootstrap
checks the source archive hashes. The export/build tools require `clang`,
`clang++`, `ld.lld`, `llvm-ar` and `llvm-ranlib`; put one LLVM toolchain on
`PATH`, or set `SYMBIAN_LLVM_BIN` to its bin directory for SDK export.

```sh
export SYMBIAN_DEPS_PREFIX="$PWD/.symbian/host-deps"
scripts/bootstrap_wheel_deps.sh
uv sync
uv run symbian doctor
uv run symbian toolchain probe
```

Build the GUI source example after preparing its headers below and selecting
an installed SDK:

```sh
uv run symbian build --project examples/gui_app \
  --output .symbian/gui-app \
  --compiler "$(command -v clang++)" --linker "$(command -v ld.lld)"
```

The report records the actual compiler and linker. Independent builds on one
host check reproducibility for that toolchain; they do not prove identical
output across macOS and Linux. `symbian init` defaults to ARMv6 and can select
ARMv5T explicitly. The cross toolchain supplies freestanding ARM EABI flags
and target include paths, so host C++ headers do not enter a guest build.

A working ARM build does not prove E32 loader acceptance, emulator execution
or Nokia 808 compatibility. The [host build guide](host-build.md) covers
native tests and wheel checks; [C++20 capabilities](../capabilities/cpp20.md)
records the bounded guest runtime.

## 3. Acquire the pinned public source profile

The checked-in manifest contains paths, hashes, revisions, and export names.
It does not contain an SDK distribution or upstream header contents.
For a fresh workspace acquire these exact public repositories, retaining their
licenses. These commands create ignored research checkouts:

```sh
mkdir -p research/upstream
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.kernelhwsrv \
  research/upstream/kernelhwsrv
git -C research/upstream/kernelhwsrv checkout \
  0c3208650587ac0230aed8a74e9bddb5288023eb
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.graphics \
  research/upstream/graphics
git -C research/upstream/graphics checkout \
  ff133bc50e6158bfb08cc093b0f0055321dcde99
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.ossrv \
  research/upstream/ossrv
git -C research/upstream/ossrv checkout \
  1e9520caca186c601dd9768449b86bc72be39a22
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.persistentdata \
  research/upstream/persistentdata
git -C research/upstream/persistentdata checkout \
  ef8baa21cee9cd1e214e1a7986595c60b3a63271
git clone --filter=blob:none --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.textandloc \
  research/upstream/textandloc
git -C research/upstream/textandloc checkout \
  59666d6704fee305b0fdd74974f7b4f42659c6a6
```

If a checkout already exists, use `git -C <directory> rev-parse HEAD` and
`git -C <directory> status --short` to inspect it; skip the corresponding clone.
Do not reset local research changes to follow this recipe. A sparse checkout
must contain every file named by the research source profile, including differently cased
`INC`/`inc` directories and private header dependencies. A full checkout is
the straightforward starting point.

Prepare the source profile:

```sh
uv run symbian toolchain prepare-gui-sdk \
  --profile research/gui_app/source-profile.json \
  --sources-root research/upstream \
  --output .symbian/gui-sdk
```

Preparation verifies each actual input's SHA-256, creates explicit header aliases
as symlinks into the original checkouts, validates the original narrow import
selection in the native core, and builds two reproducible ordinal proxies. The
migrated GUI uses the installed SDK's broader proxies at link time. It refuses
path escapes, different occupied headers, and redirected output directories.
The 93 aliases flatten SDK includes and preserve necessary `graphics/...`
namespaces without modifying upstream files. Reported repository revisions are
manifest declarations; preparation verifies file digests, not Git provenance.
Keep original licenses beside the source trees and retain those trees for as
long as the aliases are used. Symlinks are not a preserved independent copy.

The EUSER definition is `kernel/eka/eabi/euseru.def`; WS32 is
`windowing/windowserver/eabi/WS322U.DEF`. The native parser selects the 38 frozen
function ordinals without renumbering. The generated `euser.dso` and `ws32.dso`
are **link-time ordinal proxies**, not executable implementations of those
libraries. Copying them into a guest cannot supply EUSER or Window Server.
The public source profile is pre-Belle evidence, not a verified Nokia 808 SDK.
The future matched target must supply compatible real system DLLs and services.

The preparation report is `.symbian/gui-sdk/sdk-report.json`; each proxy also
retains its own source, build trees, input digests and `report.json`.
