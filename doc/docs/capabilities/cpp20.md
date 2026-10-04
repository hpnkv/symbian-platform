# C++20 on Symbian: demonstrated scope

C++20 programs are feasible with the modern ARM compiler and native E32
converter. The maintained examples compile, package and execute through the
ROMless EKA2L1 loader. This is not a complete hosted C++20 implementation or a
verified Nokia 808 runtime. Preserved Delight RM-807 ROM/Z now enables a separate
guarded SDK GUI and real libc++ string/vector runtime execution on both macOS
backends; the language/library/module
probes described here retain their ROMless scope. No phone execution has been
performed.

The [Clang language status](https://clang.llvm.org/cxx_status.html) and
[libc++ library status](https://libcxx.llvm.org/Status/Cxx20.html) describe
separate implementations. Neither an ARM ELF nor `-std=c++20` establishes
Symbian startup, library, loader or device compatibility.

## Evidence

| Feature | Local result | Boundary |
| --- | --- | --- |
| Concepts/requires, constrained generic lambdas | Compile and execute | Signed arguments fail the constraint control |
| `consteval`, designated initialization, structural class template arguments | Compile and execute | No dynamic initialization required |
| `constinit` callback tables | Compile, relocate and call | Dynamic initializers fail the compiler control |
| Defaulted equality, `char8_t`, `[[no_unique_address]]` | Compile/layout assertions | This small layout is tested, not every target ABI case |
| Named modules with a function and immediate function | CMake scanning, reproducible E32, package and execution | Upstream Clang 23.1.2; no `import std` or header units |
| `std::span`, `std::unsigned_integral`, `std::bit_cast`, `std::rotl`, `std::popcount` | Experimental headers compile; distinct E32 executes | External header configuration; no target libc++ binary |
| `<coroutine>`, `<ranges>`, `<atomic>` header smoke tests | Fail in the current header experiment | Missing target C-library declarations/types; this is not impossibility evidence |
| Heap-backed std::string/std::vector<int> | Real libc++ sources compile; E32 executes on both firmware-backed CPU backends | Primary-thread, no-exceptions subset; cleanup and allocation failure controls pass (RUNTIME.md) |
| Threads/jthread, I/O, filesystem, formatting | Not established | Require broader runtime, services and failure policy |
| Complete C++20 conformance, matched Belle, physical Nokia 808 | Not established | Remain separate milestones |

Apple Clang 21.0.0 builds the language and selected library probes. The separate
header smoke matrix uses upstream Clang 23.1.2. The module example explicitly
selects upstream Clang with `clang-scan-deps`; the Apple compiler rejected the
minimal module declaration with the tested target flags. This is a result for
these installations, not a claim about all Apple Clang versions.

## Header-free language example

```sh
uv run symbian build --project examples/cxx20_probe \
  --output .symbian/cxx20-probe
uv run symbian package --project examples/cxx20_probe \
  --artifact .symbian/cxx20-probe/cxx20_probe.exe \
  --output .symbian/cxx20-package
uv run symbian toolchain verify-pointers \
  .symbian/cxx20-probe/cxx20_probe.exe \
  --package .symbian/cxx20-package/probe.sis \
  --output .symbian/cxx20-check
```

Build the separate research oracles using [their instructions](https://github.com/hpnkv/symbian-platform/blob/main/.dev/research/eka2l1.md).
The 21 cases check Nokia's checksums/validator, four actual mapped pointer words,
ARM/Thumb callbacks, a virtual method, success/failure exits, repeat launch,
installation, registry hash/reload and uninstall/reinstall. The C++20 source
optimizes to the earlier pointer probe's exact ELF/E32 bytes. That is useful
evidence that these language features do not intrinsically need a new runtime.
Compiler controls additionally reject C++17, a signed constrained argument and
dynamic `constinit` initialization for their expected reasons.

No system C/C++ headers, hosted runtime, exceptions or RTTI are used. Constant
objects remain in the code mapping; ordinary writable data/BSS, TLS and global
constructor arrays are rejected. The startup's direct ThreadKill skips SDK
cleanup, so these examples own no target resources.

## Named modules

```sh
uv run symbian build --project examples/cxx20_module_probe \
  --compiler /opt/homebrew/opt/llvm/bin/clang++ \
  --output .symbian/cxx20-module
uv run symbian package --project examples/cxx20_module_probe \
  --artifact .symbian/cxx20-module/cxx20_module_probe.exe \
  --output .symbian/cxx20-module-package
uv run symbian toolchain verify-package \
  .symbian/cxx20-module-package/probe.sis \
  --executable .symbian/cxx20-module/cxx20_module_probe.exe \
  --output .symbian/cxx20-module-check
```

Adjust the compiler path for the installation. The example uses a `CXX_MODULES`
file set and turns scanning on for its target. The ordinary toolchain leaves
scanning off. CMake/Ninja discovers the imported BMI, retains it in the primary
build tree, and records both sources and a genuine compilation database. Editing
the exported immediate function rebuilds an unchanged importer and changes the
reproducible ELF/E32. See [CMake's module rules](https://cmake.org/cmake/help/latest/manual/cmake-cxxmodules.7.html).

The 35 checks include parser, checksum, validator, two CPU backends at two
addresses, real emulator kernel execution and package installation. Module
symbols are linked within this executable; exported module names are not
automatically an interface to historical Symbian DLLs.

An exported constexpr object caused Clang to emit an `.init_array` in a follow-up
experiment, even though its value was constant. Conversion correctly rejected
it. The maintained interface uses an immediate function. Module support does
not authorize stripping constructor arrays or skipping generated startup.

## External libc++ header experiment

The local Homebrew libc++ 23.1.2 configuration enables macOS availability markup
and host thread support. Using it unchanged for ARM fails. The isolated research
overlay disables those, monotonic-clock, filesystem, random-device, localization
and wide-character settings, and selects the serial PSTL backend. Header bodies
are unchanged. Clang's own resource headers supply basic freestanding C types;
macOS SDK headers and libraries are excluded.

This hand-edited configuration is only a header experiment. No target libc++
library is built or linked. LLVM's [vendor instructions](https://libcxx.llvm.org/VendorDocumentation.html)
require a matching library configuration when binary runtime components are
used. A supported port must generate and test its own target configuration,
ABI library, C library and runtime rather than reuse this research overlay.

To reproduce the successful subset, copy examples/cxx20_probe into a disposable
directory. Change its manifest's `cmake_preset` to `symbian-libcxx` and add an
ignored CMakeUserPresets.json there:

```json
{
  "version": 6,
  "configurePresets": [{
    "name": "symbian-libcxx",
    "inherits": "symbian-pic",
    "cacheVariables": {
      "SYMBIAN_CXX20_USE_LIBCXX": "ON",
      "SYMBIAN_CXX20_LIBCXX_CONFIG_INCLUDE": "/absolute/target/config",
      "SYMBIAN_CXX20_LIBCXX_INCLUDE": "/absolute/libcxx/include/c++/v1"
    }
  }]
}
```

The configuration directory must contain the separate experimental __config_site.
The CMake target discovers the chosen compiler's resource directory itself.
Build/package/verify this copy using the first workflow above. Compiler-discovered
dependencies hash the consumed upstream headers and separate configuration.
The library callback uses a fixed-extent span, rotates its arithmetic result and
counts input bits; its independently expressed expected result differs from the
header-free callback. It passes the same 21-case loader/installer loop.

Logs and commands for all six header smoke tests are in
.symbian/cxx20-research/headers-report.json. `<coroutine>` needs memcpy declarations;
`<ranges>` also needs mbstate_t/stdio support; `<atomic>` exposes memory/time
dependencies. No substitute definitions of standard library classes were added.

Optional tests accept explicit external inputs:

```sh
SYMBIAN_CXX20_LIBCXX_INCLUDE=/absolute/libcxx/include/c++/v1 \
SYMBIAN_CXX20_LIBCXX_CONFIG_INCLUDE=/absolute/target/config \
SYMBIAN_CXX20_MODULE_COMPILER=/absolute/upstream/clang++ \
SYMBIAN_EKA2L1_ORACLES_BUILD="$PWD/build/eka2l1" \
  uv run pytest -q symbian/tests/test_cxx20.py
```

## Maintained runtime subset

[guest runtime guide](runtime.md) now documents original LLVM string/new-helper
sources, a generated target configuration and real SDK heap adapters. This is
separate from the older external-header smoke experiment above. Strings and
vectors allocate, mutate and destruct repeatedly in real firmware-backed guest
execution; nothrow/fatal allocation paths and heap-cell cleanup are checked.
The SDK placement-new conflict is isolated behind a C ABI. Global std::nothrow,
constant reads and Thumb function references now execute through bounded local
GOT relocation on both backends; changed-value and malformed-table controls
remain maintained. See also
[application caveats](cpp.md).

## Work toward broader support

The next steps are bounded writable-data/BSS relocation, general TLS/global
and DLL initialization/cleanup, then broader C/compiler-rt services and
extensions to the configured no-exceptions libc++ subset. Existing containers
have tested heap allocation and an explicit failure policy; coroutines still
need an ownership/allocation model and event-loop integration;
atomics and threads need target synchronization primitives. None follows simply
from enabling the language standard.

Host native concurrency continues to use A11's thread library when needed.
These compiler/parser experiments remain synchronous. SDK C++ interfaces must
be checked against actual target exports/layouts; keep a modern library's types
inside code built with its matching ABI and use verified platform boundaries.
Matched Belle and physical-device checks remain necessary before claiming 808
application support.
