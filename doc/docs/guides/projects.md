# Standalone applications and a visible SDK

ROM/Z and emulator device selection now use [shared firmware onboarding](firmware.md):
global -> SDK -> project -> command overrides, explicit content IDs and offline
transfer. Physical USB discovery and SIS staging are documented separately in
[Physical device connection](device.md). Build/init require no firmware.
With the target firmware explicitly
selected during init, generated starters execute on C7, E6, 6120 and E71 as
well as the preserved 808 fixture; older Z drives select the portable profile.
New starters declare `[application]` with editable menu captions and an SVG
icon in `symbian.toml`. The icon is a project-relative path, so the project
transfers without SDK-specific or machine-specific asset locations. Add
`[application.localizations.fr]` (or another supported BCP 47 tag) with
`caption` and `short_caption` to translate the application-menu entry.
The main `[application]` captions are the fallback. The SDK maps tags to
the verified `TLanguage` numbers in `e32lang.h`, compiles all the resources
and installs them together; AppArc selects the preferred language at runtime.
There is no resource-script or SIS-language syntax in an application project.
Currently mapped tags are `en`, `fr`, `de`, `es`, `it`, `sv`, `da`,
`no`, `fi`, `en-US`, `fr-CH`, `de-CH`, `pt`, `tr`, `is`, `ru`,
`hu`, `nl`, `nl-BE`, `en-AU`, `fr-BE`, `de-AT`, `en-NZ`,
`cs`, `sk`, `pl`, `sl`, `zh-TW`, `zh-HK`, `zh-CN`, `ja`, `th`.
Unsupported tags fail before packaging. Menu translations do not yet
translate the application's own UI.

Packaging compiles original Symbian registration resources with the selected
SDK's host `rcomp`, then installs them beside the EXE. It uses `rcomp -u`
and `CHARACTER_SET UTF8` so the
original AppArc parser reads the menu path and captions. A new prepared-workspace
SDK export is needed to gain `rcomp`; the current visible SDK includes it,
the localized resource compiler policy and native SVG-in-MIF writer. Its
3,109 files passed digest audit after rebasing to `~/dev/symbian-sdk`; the
prior tree is preserved at
`~/dev/symbian-sdk-before-icon-locale-20261002`.
Screen layout and default font selection belong to the SDK template, not
application workarounds.

The visible SDK at `~/dev/symbian-sdk` contains 3,108 sealed files. The
previous 3,100-file tree is preserved at
`~/dev/symbian-sdk-before-menu-registration-20261002`. It has the
direct guest Abseil StatusOr concurrency API, Status-enabled starter, owned
property change request, bounded event mailbox and tested unresolved-`Await`
guard. The immediately preceding 3,100-file SDK is preserved at
`~/dev/symbian-sdk-before-gui-init-migration-20261002`; it and the current
tree verify against their recorded digests. The current archives also
contain the verified bounded ARM/Thumb context switch; it is not yet a public
fiber backend.
It includes 384 pinned Abseil headers and 43 compiled closure archives for
each of ARMv5T and ARMv6, the original libc++ clocks, the Symbian
monotonic-clock adapter,
the owned native timer and `TimerPump` Task adapter, selected clock/HAL and
`sched_yield` import
proxies, and the converter's
validated optional proxy subset. Its copied/relocated project test passed; an
E71 generated starter passed against the preceding clock-thread export. The
immediately prior SDK is retained at
`~/dev/symbian-sdk-before-status-concurrency-20261002`. The SDK refresh did
not rewrite the owner's application source projects. The new 3,100-file
digest set verifies; the prior SDK's 3,097 sealed files also verify, with four
additional Finder metadata files preserved. The Abseil export passed installed
Status/StatusOr/map controls on both ARM profiles and emulator backends plus
a copied-project test. The
prior timer-capability export passed both opt-in GUI controls.

The installed `Symbian::AbseilStatusOr` CMake target supplies the tested
guest Status/StatusOr, Cord-payload and `flat_hash_map` closure. Application
sources include headers such as `<absl/status/statusor.h>` and
`<absl/container/flat_hash_map.h>`; CMake links the target with the matching
`Symbian::Streams` runtime and selected OS proxies. The SDK also installs
its startup bridge under `share/symbian/runtime`, original headers under
`include/abseil` and per-architecture archives under `lib/<arch>/abseil`.
See `examples/abseil_status_probe` for a real E32 project using the installed
target. Set `SYMBIAN_ABSEIL_USE_SDK=1` for its `symbian-sdk` preset, or use
`symbian-pic` with a clean pinned Abseil checkout. This is a bounded tested
subset; selected guest Abseil time now executes, while general TLS and the
full library remain open.

`TimerPump` is provided by `<symbian/concurrency/timer_pump.h>` under
`Symbian::Stackless`. It supplies bounded monotonic timer Tasks and a
coalesced cross-thread cancellation wakeup. `symbian init` now enables guest
Abseil Status/StatusOr and timer Tasks by default. This uses the one Window
Server/timer request-semaphore loop, logs immediately and again after a
delayed Task, and cancels pending Tasks on Clear. The profile needs firmware
`libpthread.dll`; `init --firmware` selects the portable profile when that DLL
is absent from drive Z, and `--portable-runtime` selects it explicitly.
Run preflights an imported `libpthread.dll` before starting the emulator and
gives a rebuild instruction when it is unavailable. The visible SDK uses
`absl::Duration` for relative timers and `absl::Time` for absolute deadlines.
It converts accepted absolute times once to monotonic relative waiting, so a
later device-clock correction does not move an in-flight timer.
The timer pump now admits at most 64 pending native timer Tasks by default;
its constructor accepts another limit. Saturation produces an already-ready
Task with `kResourceExhausted`, and dispatching a completed timer frees a slot.
The default is a provisional resource policy, not a device timer limit.

The shipped stackless profile also executes `TaskGroup` joining and cancelling
real timer Tasks in the installed guest matrix. That integration remains
specific to the verified timer request owner.

The current installed profile also supplies `PropertyWatch::Next()` as a
`Future<int>` over an owned `RProperty::Subscribe` request. It shares the one
event-thread wait with timers. `EventMailbox` admits bounded cross-thread UI
callbacks and executes them outside its lock; the generated timer starter uses
it for delayed UI work. This mailbox is an event-affinity route, not A11's
shared-pool `Post`. The current `Await` guard returns ready values and rejects
unresolved waits until a fiber-aware backend exists.

The development SDK is installed at `/Users/helena/dev/symbian-sdk` on this
host. The existing `/Users/helena/dev/symbian-app` project now selects it through
`sdk-location.json`; application sources and UID were preserved.

Create a project from any working directory:

```sh
symbian init ~/dev/hello_time
```

The wizard asks for the application name, target architecture, IntelliJ/CLion
integration and an optional experimental UID. ARMv6 is the default; use
`--architecture armv5t` for an ARMv5T target. It builds initially, so code analysis, the compilation
database and debug symbols exist before the first IDE opening. For scripts:

Generated projects include `.clang-format` with automatic braces around every
C++ condition and loop body. Run `clang-format -i` (version 15 or newer) on
edited C++ files to apply the project style.

```sh
symbian init ~/dev/hello_time --name hello_time --non-interactive
symbian app build --project ~/dev/hello_time
symbian app run --project ~/dev/hello_time
```

`--ide none` omits IDE configuration; `--no-build` defers the initial build.
An existing nonempty project is refused. The starter displays hello world,
appends local system time on taps, bounds the log to twelve lines, and provides
Clear/Exit controls. Backspace clears and Escape exits. Focus loss suspends tap
handling; native requests are cancelled and drained before their stack storage,
model and handles are destroyed. This fixed-orientation example has no persistent
storage or application-launcher registration. It is tested against the preserved
Delight fixture, not a physical phone.

Application logic now lives in typed `model.h`/`model.cc`; the SDK-owned
`app_bridge.cc` holds the C ABI boundary to the platform GUI. The runtime
supports a bounded `std::thread` path when the selected ROM provides
`libpthread.dll`; `Symbian::Threads` links its standard imports. The portable
starter profile remains available on devices without that DLL; the selected
firmware drives this choice during `init --firmware`. Guest exceptions remain
off by default; an opt-in
exception profile requires unwind/descriptor execution tests.

The bounded guest stackless completion headers are exported under
`include/symbian/concurrency`; `Symbian::Stackless` links pinned guest
Abseil, the matching streams runtime and selected proxies. Use it for
producer-completed A11-style
Promise/Future/Task, nonblocking `JoinAll`, bounded `DriveInline` and
`TaskGroup::Finish`. It uses `absl::StatusOr<T>` directly, provides a bounded
event-affinity mailbox and rejects unresolved `Await` outside a guest fiber;
inside a fiber it parks cooperatively. `Symbian::Stackless` links the installed
`Symbian::Fibers` archive because its lock/channel headers can park fibers.
The bounded fiber scheduler is not yet A11's pool or Select API. The current
visible SDK verifies by
digest and passed eight installed stackless controls on both ARM profiles and
both emulator backends. Its guest mutex, condition variable and bounded
channel use the `thread::` namespace; Future/Task ownership remains staged
under `symbian::concurrency`. The full A11 fiber/select API and its header
tree remain open. The earlier 2,496-file tree is
retained at `~/dev/symbian-sdk-before-stackless-20261001`.

The preceding timer-owner export added
`<symbian/concurrency/native_timer.h>` and selected EUSER timer imports. Its
2,595 payload files verified by digest; the
previous sealed SDK is at
`~/dev/symbian-sdk-before-native-timer-20261001`. Source, installed candidate
and canonical smoke execution exercised the owned timer request on the named
RM-807 emulator fixture. The subsequent 2,596-file SDK adds the bounded
timer-to-Future path described above.
The visible SDK was refreshed after the threaded-runtime controls: its 2,496
payload files verify by digest, and the previous 2,428-file tree is retained at
`~/dev/symbian-sdk-before-threads-20261001`. The refresh did not rewrite the
owner's application projects.

The earlier exception-metadata converter update produced a digest-verified
2,496-file export. Its prior tree is retained at
`~/dev/symbian-sdk-before-exception-metadata-20261001`; owner application
projects were not regenerated. This update does not enable guest exceptions
for generated applications.

The current visible SDK exports the Clang/OpenC varargs adapter at
`include/config/stdarg_e.h` and original libc++ error-category code in its
ARM runtime archives. A separate `Symbian::Streams` target now selects
the classic-C locale/iostream configuration and a matching ARM archive;
it replaces `Symbian::Runtime` for targets needing bounded stream output.
An earlier converter export added validated imported-function pointers and
LLD ARM long-thunk relocation. Its 2,535 sealed files verified after
promotion; the previous 2,503-file tree is at
`~/dev/symbian-sdk-before-streams-20261001` and also verifies. The
earlier C-service export is retained at
`~/dev/symbian-sdk-before-runtime-c-services-20261001`. Owner application
projects were not regenerated.
The final parser safety check was exported and verified once more; the directly
preceding SDK tree is at `~/dev/symbian-sdk-before-exidx-validation-20261001`.

New starters acquire the model with `new (std::nothrow)`. On null they close
the window/group, release the font and return KErrNoMemory through startup.
Both CPU backends pass a real heap-limit failure control. Later string/vector
growth retains the runtime's fatal ordinary-allocation policy. Local GOT
relocation is supplied by SDK conversion; applications need no local tag trick.

## Install and select the SDK

Export from the prepared research checkout for the first installation:

```sh
symbian sdk install ~/dev/symbian-sdk --workspace ~/dev/symbian
```

Once an SDK is active, another installation can copy it without the checkout:

```sh
symbian sdk install ~/dev/another-sdk
```

The destination must be new. Installation records an active user setting under
`~/.config/symbian/active-sdk.json` (or `$XDG_CONFIG_HOME`). An explicit `init
--sdk /path/to/sdk/sdk.json` takes precedence, followed by
`SYMBIAN_SDK_MANIFEST`, then that active setting. Existing projects retain their
own selection. Repository edits do not update an existing SDK export. Export
to a new prefix with `--workspace` to get current implementation/templates;
keep the prior tree and verify its recorded digests before replacing it. The
earlier GOT checkpoint refreshed `~/dev/symbian-sdk` through a fresh export and
retained its previous tree at `~/dev/symbian-sdk-before-local-got-20261001`.
The latest ARMv6/ROM/Z/static-archive update published the 2,420-file verified
payload and retained the previous visible tree at
`~/dev/symbian-sdk-before-arm-profiles-20261001`. It did not regenerate the
owner's three application projects. The installed SDK generated and initially
built an ARMv6 E6/RM-609 application in `.symbian/installed-e6-smoke`.
Refresh generated build/IDE configuration with:

```sh
symbian app configure --project ~/dev/symbian-app --sdk ~/dev/symbian-sdk
```

This preserves application source files but regenerates CMake, presets, manifests,
README and saved IDE integration. Preserve custom changes to those generated
files before using it. The only local project setting is `sdk-location.json`:

```json
{"sdk": "../symbian-sdk"}
```

This is a path, not a version or digest lock. Change it to another installed
SDK and run `symbian app build --project PROJECT`; the CLI refreshes the
compiler identity and CMake cache when necessary. The generated `sdk.cmake`
also makes CMake watch `sdk-location.json`, so a direct CMake/Ninja build
reconfigures after the path changes. The installed SDK's own `sdk.json`
describes its tools and runtime archives; applications do not carry a copy.
The large source-selection manifest for the maintained GUI research example
lives separately at `research/gui_app/source-profile.json`.

Absolute SDK locations are accepted too. The setting is ignored by Git; collaborators
provide their own. Project CMake, proxy references, launchers and IDE macros stay
relative. Move a project and rebuild: its previous CMake cache belongs to the
old source location and must be removed. Installing a copy of the SDK rebases its
manifest; merely moving an SDK directory does not currently rebase `sdk.json`.

| SDK directory | Contents |
| --- | --- |
| `include/platform` | Materialized selected original SDK headers, including `w32std.h` and its dependency closure |
| `include/c++`, `include/config` | Pinned libc++ headers and matching generated guest configuration |
| `include/compiler`, `include/openc`, `include/libm`, `include/libc`, `include/pthread`, `include/posix4` | Compiler resource and preserved target C/math/pthread declarations |
| `lib/armv6`, `lib/armv5t` | Separate built guest runtime archives; the root archive supports older projects |
| `proxies` | Selected frozen-ordinal import libraries for EUSER, WS32, GDI, libc, libm, libpthread and drtaeabi; only actually used imports become E32 dependencies |
| `cmake` | ARM toolchain and `Symbian::Runtime` package |
| `lib/python` | Observable SDK Python utilities and actual native extension modules |
| `bin`, `libexec` | Compiler/linker/LLVM archive/Python/GDB wrappers, CLI and Python bootstrap |
| `licenses`, `provenance.json`, `digests.json` | Notices, source identity and file hashes |
| `host-dependencies.json`, `sdk.json` | Explicit external prerequisites and installed entry points |

The pip command dispatches project builds/runs into that project's SDK CLI.
The project launchers read the same local setting without importing the pip
package first. SDK Python bootstrap also prevents an editable checkout's import
finder from overriding SDK utilities or native modules.

Generated `symbian.toml` lists the SDK's selected proxy catalog. The ARM link
keeps only proxies whose symbols the image actually imports, and E32
conversion validates the resulting needed subset against that catalog. A
declared but unused libc, math or thread proxy is therefore not an extra
firmware dependency. Selecting a target that actually uses one of those
services still requires that DLL and its verified ordinals on the chosen ROM.

The current development export still requires the declared host Python and its
third-party packages, LLVM/LLD, ARM GDB, patched EKA2L1 and private firmware at
existing locations. It does not yet bundle those full host payloads. Target
headers and the runtime are actual copies, not research-tree symlinks.
The single-install payload design and remaining packaging gates are in
[DISTRIBUTION.md](https://github.com/hpnkv/symbian-platform/blob/main/.dev/distribution.md).

An application may contain a separate ARM static library. Add sources in the
project, then use the installed CMake helper:

```cmake
symbian_add_static_library(my_logic SOURCES my_logic.cc)
target_link_libraries(my_app PRIVATE my_logic)
```

The helper applies the same target headers, ABI flags and runtime interface as
the app. The SDK selects LLVM `ar`/`ranlib`, records their versions in build
identity and keeps DWARF paths stable across reproducible builds. Debug the
library through an app that links it; the final ELF supplies its source symbols.
The two target-architecture integration tests build and link the archive into a
generated application and inspect its ELF/DWARF symbols. An installed SDK also
exposes `symbian_add_dynamic_library` in `SymbianPic`. It accepts C or C++
sources, startup, linker script, frozen DEF and UID3; its target publishes a
`.dll` and retains `<target>_elf.elf` for symbols. `IMPORT_SYMBOLS` creates
`<target>_import` for consumers and `<target>_proxy` for explicit proxy builds.
`IMPORT_PROXIES` links selected external proxies and passes their exact bytes
to native E32 conversion. A C DLL and consumer import build on ARMv5T and
ARMv6. The SDK's default DLL startup and linker layout link the guest runtime
and selected EUSER proxy without hand-written entry assembly. An independent
emulator probe verifies per-process writable DLL state on both CPU backends;
a C++ constructor now runs on process attach on both architectures/backends,
with a changed-constructor control. A bounded dynamic `RLibrary` client also
loads the DLL, looks up two ordinals and observes its destructor before close
returns; missing-DLL loading reports an error. TLS, broader DLL lifetime, module
symbol loading and complete debugging remain open.
The installed EUSER proxy now includes the tested `RLibrary::Load`, `Lookup`
and `Close` ordinals, so generated clients use the ordinary SDK import path.
Other firmware profiles still need their own import/loader execution checks.

The Mbed TLS adaptation's full
`mbedcrypto` C archive compiles with the SDK C compiler; a SHA-256 subset links
and converts as a DLL with selected EUSER imports. That subset now executes
through a dynamic `RLibrary` client on both emulator CPU backends, including a
changed-input failure control. This does not establish the whole Mbed TLS
feature set or a complete TLS service stack.

The maintained optional integration runs with
`SYMBIAN_MBEDTLS_SOURCE=~/dev/mbedtls-symbian` and `SYMBIAN_APP_SDK` pointing to
the installed SDK manifest; its source stays in the external project.

```cmake
project(my_library LANGUAGES C CXX ASM)
include(SymbianApp) # Imports SymbianPic and Symbian::Runtime.
symbian_add_dynamic_library(my_library
  STARTUP startup.S LINKER_SCRIPT image.ld SOURCES api.c
  EXPORT_DEFINITION exports.def UID3 0xE0001234
  IMPORT_SYMBOLS MyPublicFunction
  IMPORT_PROXIES "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso")
target_link_libraries(my_library_elf PRIVATE Symbian::Runtime)
# In the same CMake project, consumers link my_library_import.
```

The DEF ordinals and UID are intentional API/identity choices. A linker script
must separate code from writable data/BSS and place any ELF import metadata in
the code mapping; see the tested data and import probe layouts. The helper
checks its inputs and converts only after ELF link succeeds. A missing selected
import or unsupported relocation fails at link/conversion, with no partial DLL.

## IDE analysis, Run and Debug

Open the generated `CMakeLists.txt` as a CMake project. The saved `Symbian App`
profile is enabled, selects Ninja/Debug and the project's SDK compiler. The CLI keeps its `symbian-pic` preset. A stable ordinary IDE profile avoids
version-dependent configure/build preset profile IDs. A named
application-wide “Symbian ARM” toolchain is unnecessary. `sdk.cmake` executes
before `project()`, so even a normal CMake configure gets the correct compiler.
`Symbian::Runtime` supplies include directories, ABI definitions and flags through
`target_link_libraries`; IDEs consume that actual target configuration. Do not
add a separate list of IDE-only include paths.

The wizard registers the CMake workspace and a C++ module, including when
using the CLion plugin in IntelliJ IDEA; generating only Run XML files leaves
that IDE opening a generic module. The repaired live app-3 import reports three
sources and zero unknown sources.

`app.cc`, `model.cc` and startup sources belong to the executable target. The
root `compile_commands.json` link and `.clangd` expose the same compilation
database to other editors. Both the W32 translation unit and the modern C++ model
pass clangd analysis with zero errors on the configured owner project. Go To
uses the visible original headers and standard library headers.

Choose **App Run**. Its executable is the project's `sdk-run`, not an ARM ELF
that macOS would try to execute. The supervisor publishes matching ELF/E32,
copies the resolved verified firmware baseline, launches the emulator and retains logs/reports under
`.symbian/runs/`. Exit completes normally; Stop terminates and reaps the owned
emulator. The saved executable was exercised independently of the IDE with real
GUI input and normal exit.

Choose **App Debug**, put a breakpoint in `AppLogTime`, continue and tap. The
`sdk-debug` wrapper starts a halted emulator before ARM GDB, using the configured
port, and installs post-connection symbol relocation from the actual runtime
mapping. Debug symbols use the current project's source paths. Real GDB source
breakpoints, clock arguments and a backtrace are tested. The live CLion debugger
frontend and full unwinding remain separate unverified gates.

For a project already open while its settings were refreshed, close and reopen
it, then reload CMake if needed; an in-memory IDE model does not automatically
prove it loaded an externally edited workspace. Saved files and compiler analysis
are verified, but toolbar interaction has not been automated.
