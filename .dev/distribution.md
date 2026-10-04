# One install for the SDK

The release goal is `python -m pip install symbian-platform` on supported macOS
and Linux hosts, followed by `symbian doctor`, project creation and an IDE-ready
build. That command should resolve the native Python library, platform headers,
import libraries, compiler tools, debugger and EKA2L1 automatically. This is a
release design, not a claim that the current wheel already contains those tools.
The current wheel contains the host format libraries and Python orchestration.
Firmware onboarding is now implemented through a shared, verified store and
portable bundles; see [FIRMWARE.md](../doc/docs/guides/firmware.md). New SDK exports contain no
firmware location. The pinned GPL native import helper belongs with the future
emulator payload and currently remains an explicit external dependency. Mutable
SDK-level emulator preferences are excluded from immutable SDK payload digests.

The visible development SDK now installs a tested subset of the A11-pinned
guest Abseil source: 384 headers, 43 closure archives for each of ARMv5T and
ARMv6, a CMake target, license and provenance. The source checkout is required
to produce a fresh SDK export, but an installed application project builds
from the SDK alone. This is not yet a published wheel or a complete Abseil
capability claim. Its platform patches remain replayable under
`research/abseil/`.

Use several exact-versioned wheels behind one install command. Keep the large
host executables independent of the CPython ABI, so upgrading Python does not
repeat the compiler/emulator payload. Wheels are already the installation unit
for compiled software; end users should not need to compile the SDK during
installation. [Wheel specification](https://packaging.python.org/en/latest/specifications/binary-distribution-format/),
[packaging flow](https://packaging.python.org/en/latest/flow/).

## Payload layout

Names below illustrate the dependency graph; they have not been registered or
published. The umbrella release pins a compatible component set. Dependencies
are normal wheel dependencies, not post-install downloads or install hooks.

| Distribution | Contents | Compatibility |
| --- | --- | --- |
| `symbian-platform` | CLI, Python policy, statically linked host parsers and genuine A11 status bindings | CPython ABI + host OS/architecture |
| `symbian-sdk-data` | Materialized public headers, frozen export metadata, generated function import proxies, CMake modules, examples, provenance and coverage | Python 3, no host ABI; named guest SDK profile |
| `symbian-llvm` | Clang, clangd, LLD ELF driver, LLVM inspection/archive tools, builtin resources and necessary shared libraries | Python 3, host OS/architecture |
| `symbian-eka2l1` | Pinned, patched emulator, Qt/SDL and private dependency closure | Python 3, host OS/architecture |
| `symbian-gdb` | ARM-capable GDB, private embedded Python and its closure | Python 3, host OS/architecture |
| Existing `cmake` / `ninja` wheels | Host build orchestration | Their maintained platform support |

The full SDK should be the default install, rather than requiring a newcomer to
work out extras. A separately named host-only package can serve parser-only use
if needed. Model component manifests, installed locations and project locks
with Pydantic `BaseModel`. Package `__init__.py` files provide public shortcuts;
implementation belongs in modules. Native binary/format handling stays in C++.

The current A11 status integration also installs the canonical
`pybind11_abseil.status` and `pybind11_abseil.ok_status_singleton` modules. They
must remain present. Before publishing alongside A11, give this shared runtime
its own compatible, versioned distribution: two projects must not independently
own the same installed files. The pin and Abseil ABI must agree. Do not rename
or omit those modules while continuing to use the canonical upstream casters.

An architecture-independent wheel containing an executable is incorrect. Use
platform tags on compiler/emulator/debugger payload wheels, even when they have
no Python extension. The small extension initially remains `cpXY-cpXY`; an
`abi3` claim requires separate pybind11/CPython compatibility work.
[Compatibility tags](https://packaging.python.org/en/latest/specifications/platform-compatibility-tags/).

## Measurements on this host — 2026-10-01

Private reports are in `.symbian/distribution-research/`; they contain local
paths and are not release artifacts.

| Current component | Observed size / constraint |
| --- | --- |
| Previous host wheel | About 2.3 MiB; adding the complete status runtime requires a new measurement |
| EKA2L1 application bundle | 112,632,329 regular-file bytes, 184 regular files, 36 symlinks |
| Gzip-compressed application | 40,956,973 bytes; local measurement, not a finished wheel |
| Current GUI header selection | 92 aliases, 2,310,735 materialized bytes |
| Current GUI proxies | 10 EUSER + 28 WS32 function imports; not the complete SDK |
| Installed Homebrew LLVM tree | About 1.8 GiB; copying the whole prefix is unsuitable |
| ARM GDB closure | Includes Homebrew Python 3.14, readline, zstd, ncurses, lzma, MPFR and GMP |

The relocated application exits successfully for `--help` with PATH limited to
`/usr/bin:/bin`, a fresh private data root, and no inherited Qt/DYLD/EKA settings.
Its 33 inspected Mach-O binaries have no non-system absolute linked dependency.
This is a relocation smoke test, not a relocated GUI/debugger acceptance test.
The largest declared macOS minimum across that closure is **26.0**; the current
host extension also declares 26.0. Do not retag those binaries for an older OS.
An older floor needs rebuilt dependencies and execution tests at that floor.

PyPI's current defaults are **100.0 MB per uploaded file** and **10.0 GB per
project**. The measured emulator appears to fit, but the LLVM and GDB release
payloads are not measured yet. Keep each tool closure complete; request a limit
increase if necessary rather than splitting a single tool into partial,
unusable installations. Track total retention across release/ABI/platform
matrices. [PyPI storage limits](https://docs.pypi.org/project-management/storage-limits/).

## Build the tools as distributions

LLVM should be built from a pinned upstream source with selected distribution
components and ARM as the required guest backend. Include clangd and the tools
used by diagnostics, not just clang. Do not copy Apple Clang, Apple's SDK or a
Homebrew prefix into a public package. Build a relocatable runtime closure and
keep licenses/source references. LLVM documents selective distribution builds.
[LLVM distribution builds](https://llvm.org/docs/BuildingADistribution.html).

Preserve the LLD driver's invocation name `ld.lld`: resolving a symlink to a
generic `lld` executable can change driver selection. Verify actual ARM ELF
and Symbian E32 results using the existing validator and emulator oracles.

The existing EKA2L1 macOS bundle already carries Qt and SDL. Release automation
must reproduce that deployment, patch set and dependency closure from a pinned
source/submodule manifest. Ship EKA2L1 as a separate process. Keep the GPL control
adapter in the emulator build; do not link it into the host Python library.
[Qt macOS deployment](https://doc.qt.io/qt-6/macos-deployment.html).

The currently working debug path uses a Python GDB hook to relocate symbols
from the actual guest mapping. The first packaged debugger should retain that
behavior and bundle its embedded Python runtime/stdlib independently of the
CLI's Python version. Set its private Python home only for GDB. A GDB executable
without that closure is insufficient. A later native `qOffsets` implementation
could remove the hook/runtime need, but the pinned EKA2L1 stub does not implement
it yet. Verify segments and ARM/Thumb transitions before changing this contract.
[GDB remote offsets](https://sourceware.org/gdb/current/onlinedocs/gdb.html/General-Query-Packets.html).

## Headers and import libraries

Current staging symlinks into preserved upstream checkouts. A wheel must contain
real file contents, preserved licenses and a per-file source/hash inventory.
Headers alone are not an SDK: distribute matching frozen export/ordinal data,
function proxies, compiler configuration and resource/build tools for each
supported guest profile.
Static archives require the target LLVM `ar` and `ranlib`, independent of the
host's default archive tools. Development exports now declare and wrap them;
their versions participate in target build identity. Dynamic libraries also
need frozen export descriptions and verified per-process data/lifetime handling.

The native proxy generator currently selects 1–256 function exports and rejects
DATA/absent exports. Bulk library generation and complete library coverage must
be added and tested before advertising all platform headers as usable. Start
with the demonstrated EUSER/WS32 profile; list uncovered APIs explicitly. DATA,
TLS, Avkon resource compilation and hosted C++ library support remain separate
capabilities. C++20 language support does not provide a hosted C++20 runtime.
The development export now includes an EPL-pinned `rcomp` host binary, its
64-bit host patch provenance, original `AppInfo.rh` and a UID checksum helper
for tested application-menu registration. The host tooling now also packages
project SVG icons and BCP 47 keyed menu captions through a bounded native
MIF/SIS writer, with original EKA2L1 installer/AppArc/MIF reader controls.
Physical Belle icon rendering, general Avkon resources and the future
multi-platform wheel payload remain open.
The host MIF writer links the host zlib provider. The macOS wheel's
`/usr/lib/libz.1.dylib` dependency passed the installed-wheel dependency
and resource audit. Cross-host byte identity and Linux wheel dependency
closure for this new writer still require an audit.

Expose CMake targets such as `Symbian::euser` and `Symbian::ws32` from an installed
`find_package(Symbian CONFIG)`. The SDK can make all supported libraries available
while projects link only their declared dependencies. The current GUI's 38
selected imports call the real system DLLs; no replacement full system library
is linked into its executable.

## Installed resources and runtime state

Discover components through installed distribution metadata/resources and a
versioned manifest. Resolve explicit user tool overrides separately; do not
silently fall back to Homebrew when the installed closure is broken. Generate
CMake user presets, clangd context and CLion run/debug configurations from those
resolved paths. Avoid checkout paths and `/Users/helena` in release resources.

Keep immutable package data in site-packages. A bundled application archive may
preserve framework symlinks that wheel extraction does not promise to preserve.
Extract a digest-checked archive into a private, versioned cache using a lock,
staging directory and atomic publication; reject absolute paths, escaping links,
and duplicate/ambiguous archive entries. Never mutate installed package files
or execute an installer script on `pip install`. All tool payloads must already
be available from the resolved wheels for offline use.

Use per-user cache/data/config locations on each host. Writable emulator
instances, firmware imports, logs, screenshots and project artifacts stay
separate from the installed payload. A project lock records component versions,
SDK profile and fixture digests. Associate debug symbols with the exact published
E32 per session, not a global mutable artifact that another launch can replace.
Retain bounded ownership, failure reports and the current shutdown-timeout
investigation.

The owner-supplied Delight firmware has no established redistribution grant.
**Do not bundle ROM/Z images or private Nokia firmware/device data.** Building
can work immediately after install; booting the matched firmware experiment
requires a one-time, explicit local import. Import is a host operation, not a
phone-flashing API. Physical identity/recovery compatibility remain unknown.

## Linux from the same source tree

A11's inspected implementation uses isolated static dependency prefixes,
Ninja/CMake presets, scikit-build-core, and cibuildwheel manylinux_2_28 builds
for x86-64 and AArch64. Its tests separately verify native GTests and installed
wheel behavior; Mach-O/ELF dependency audits reject developer-machine libraries
and absolute loader paths. Carry those patterns over, not its unrelated network,
audio dependencies. The current synchronous tools need no scheduler. The root
host build now exposes one shared channel/Future/Task source layer over a
private Boost.Fiber backend. Its opaque public header
and ordinary consumer link have no separate Boost dependency: a single SDK
static archive bundles the required Boost.Fiber/Context object code. Fresh
development SDK exports build that archive from the pinned A11 host adaptation
and copy it under `lib/host/<system>-<processor>/`, with Boost-free headers in
`include/host/thread` and a `Symbian::HostConcurrency` CMake import. Boost is
needed to rebuild this archive, not to use the exported SDK. The host backend
now includes a custom ready-fiber scheduler, stackless shared-pool `Post`/
`PostAt`, and `PermanentEvent`/`Select`; the installed-wheel Python boundary
provides a bounded asyncio Future bridge and deferred-reference drainage.
Full A11 fiber-tree, work-stealing and shutdown parity remain distribution
gates. The macOS Python extension now links the bundled host object code for
its scheduler/asyncio bridge, but the standalone host archive is not a wheel
SDK payload. Linux static-archive execution and wheel audit remain open.
Host Abseil remains a separate CMake
dependency for this development export.

The development SDK also materializes `lib/<arch>/libsymbian_guest_fiber.a`
and exposes `Symbian::Fibers` after the guest Abseil closure is built. This
guest archive uses the owned ARM switch, not Boost. Its installed ARMv5T/ARMv6
emulator tests cover cooperative locks, condition waits, Future await,
timeouts and a custom scheduling policy; native request-loop integration
remains open.

This repository now has host presets, `SYMBIAN_DEPS_PREFIX`, a trimmed A11-style
static OpenSSL bootstrap, genuine status runtime installation, installed-wheel
Mach-O/ELF audits and a Linux host CI matrix. See [HOST_BUILD.md](../doc/docs/guides/host-build.md).
Those are host-library gates. Linux compiler payloads, emulator GUI/control,
GDB and CLion integration need their own gates before a full Linux SDK claim.

Linux extension wheels target glibc 2.28 initially, on x86-64 and AArch64.
Use native CI runners where available; an emulated build is still useful but
must record that execution environment. Auditwheel repair applies to extension
libraries; separately inspect every executable, Qt plugin and shared library in
payload archives. Do not assume it validates executables hidden in an archive.
A Qt GUI also needs declared system display/graphics requirements, plugin paths,
and tests under Xvfb and real X11/Wayland sessions. An offscreen capture alone
cannot establish interactive GUI usability. Musl/Alpine is a separate future
support tier. [cibuildwheel](https://cibuildwheel.pypa.io/en/stable/options/),
[manylinux policy](https://github.com/pypa/manylinux).

## Release acceptance

1. Build/audit the small host wheel on each Python/host architecture, then
   install it in an empty environment and run the native status/parser/resource
   checks outside the checkout. GTests and wheel tests are separate gates.
2. Build each portable tool closure from pinned sources; record component and
   archive digests, source/submodule revisions, patch hashes and build settings.
3. Install the full suite on a clean host without Homebrew, SDK checkouts or
   compiler tools. Disable network access after installation. Build the copied
   GUI with materialized headers and CMake targets, then run/debug it against a
   separately imported fixture and compare native exit records and pixels.
4. Exercise paths with spaces/non-ASCII characters, read-only site-packages,
   fresh and concurrent caches, virtualenv deletion/recreation and upgrades.
   Check source breakpoints, variables and GDB MI plus the actual CLion frontend.
5. Publish only after per-component license/source obligations are satisfied.
   Preserve A11/Abseil/pybind11/nlohmann/OpenSSL notices, Symbian source licenses,
   LLVM notices and corresponding EKA2L1/GDB/Qt sources and build/patch material.
   Review bundled Qt modules individually; a process boundary alone does not
   decide licensing obligations. Root project licensing also remains to be
   defined before public distribution. Sign/notarize the macOS executable
   closure in the final layout and test it on a fresh machine.

[EKA2L1 license](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/LICENSE),
[Qt licenses](https://doc.qt.io/qt-6/licensing.html),
[Apple notarization](https://developer.apple.com/documentation/security/notarizing-macos-software-before-distribution).
