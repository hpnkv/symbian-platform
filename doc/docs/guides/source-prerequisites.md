# Prepare the toolchain and public source

All commands in this document start in this repository's root unless indicated.
Use Apple Silicon macOS with Apple's command-line development tools installed.
Check the selected developer directory and compiler:

```sh
xcode-select -p
xcrun --find clang++
xcrun clang++ --version
brew install uv cmake ninja lld llvm googletest openssl@3
uv sync
uv run symbian doctor
```

Install Apple's command-line tools separately if `xcrun` cannot locate Clang.
`uv sync` builds the host native extension and installs the project CLI and
development tools. `doctor` reports tool availability and target readiness; a
working host toolchain does not make the Belle runtime ready.

The tested GUI build used Apple Clang 21 and LLD 23.1.2. Use the default
`clang++`/`ld.lld` discovered on PATH, or select them explicitly:

```sh
uv run symbian build --project examples/gui_app \
  --output .symbian/gui-app \
  --compiler "$(xcrun --find clang++)" \
  --linker "$(brew --prefix lld)/bin/ld.lld"
```

Run that build only after preparing the source headers in the next section and
activating an installed SDK. The migrated GUI links the installed SDK's
`Symbian::Stackless` target and selected import proxies; the earlier staged
EUSER/WS32 proxies remain provenance controls for the original narrow GUI.
Homebrew can install newer versions than this checkpoint; build reports record
actual paths and versions. Reproducibility currently means independent build
directories on the same host and toolchain, not identical output from every
compiler release.

Current new projects use the SDK's `symbian-arm.cmake` toolchain with
`SYMBIAN_TARGET_ARCH=armv6`. `symbian init --architecture armv5t` selects the
older target when needed. Existing ARMv5T projects retain their choice.
Effective C++ flags include `--target=armv6-none-eabi`, `-mthumb`,
`-mfloat-abi=soft`, `-mabi=aapcs`, `-ffreestanding`, `-std=c++20`,
`-fPIC`, `-fno-exceptions`, `-fno-rtti`, and `-nostdinc`.
The GUI adds `-g -gdwarf-4 -O1`; its final `-O1` overrides the generic `-O2`.
Target code cannot accidentally include macOS C++ headers. SDK includes are
supplied explicitly. `_UNICODE`, the GCC/EABI compatibility definitions,
`__EPOC32__`, and ARM platform definitions select the upstream headers' intended
declarations. The linker retains relocations and rejects unresolved imports.

C++20 language support is real, but a complete target standard library is not
available. This GUI uses neither a hosted libc++ nor exceptions. See
[C++20 guide](../capabilities/cpp20.md) for the separately tested language, modules and
selected header-only library experiments and their remaining runtime work.

# 3. Acquire the pinned public source profile

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

