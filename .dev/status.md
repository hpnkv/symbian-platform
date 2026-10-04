# Status

2026-10-04 provisional Linux host CI gate: [run `37202283905`](https://github.com/hpnkv/symbian-platform/actions/runs/37202283905) passed all four jobs on commit `23b2133`. Ubuntu 24.04 x86_64 and aarch64 built with CMake/Ninja and passed 10/10 CTests each. Separate manylinux_2_28 x86_64 and aarch64 jobs built and auditwheel-repaired CPython 3.11–3.14 wheels; all eight wheels installed in fresh environments and passed the installed-wheel audit (native import, CLI doctor, packaged resources and ELF dependency allowlist). The aarch64 wheels and Linux Python 3.14 wheels omit automatic pywebview/PySide6 because the tested manylinux baseline could not resolve that renderer. This is host core/package evidence. No interactive Linux source SDK export, EKA2L1 GUI, guest GDB, Console visual run, USB device test or Nokia 808 compatibility was established. Documentation workflow `37202283842` built and deployed successfully on the same commit.

2026-10-04 Linux aarch64 wheel gate: run `37201321281` built and auditwheel-repaired CPython 3.11, 3.12, 3.13 and 3.14 manylinux aarch64 wheels. Each installed in a fresh virtual environment and passed `python -m symbian.build_support.audit`, including native import, CLI doctor, resources and ELF dependency checks. The same run passed the Ubuntu 24.04 native build and 10/10 CTests on both aarch64 and x86_64. The aarch64 wheel excludes the unverified pywebview/PySide6 Console GUI dependency; an interactive Linux host, source SDK export, emulator and physical device remain separate gates.

The x86_64 wheel job in `37201321281` passed installed-wheel audits for CPython 3.11–3.13. Its CPython 3.14 wheel built and auditwheel repaired, but installation failed because no PySide6 wheel compatible with both Python 3.14 and manylinux_2_28 was available. The package metadata and lockfile now limit automatic Linux pywebview/PySide6 installation to x86_64 with Python 3.11–3.13; Python 3.14 retains the host CLI without a verified Console renderer. The full rerun remains pending.

2026-10-04 Linux CI dependency finding: run `37199385640` failed all four host jobs at CMake configure because neither the native isolated prefix nor the manylinux wheel prefix contained Boost's CMake package. OpenSSL and libusb bootstrap had passed. The bootstrap now hash-pins Boost 1.90.0, builds static Fiber/Context and supporting components with A11's explicit host architecture/ABI settings, and installs its license with the Python wheel. The manylinux_2_28 x86_64 image has GNU `ar` but no `llvm-ar`; a container probe confirmed its MRI `-M` mode merges an archive. The host bundle therefore uses GNU `ar` on Linux if LLVM's tool is absent. `bash -n`, `git diff --check`, local macOS CMake build and 11/11 CTests passed; the new CI run remains the Linux validation gate. No Linux native build, wheel, emulator, or Nokia 808 compatibility result is inferred from this setup fix.

Linux CI run `37199620567` built and installed the pinned Boost on Ubuntu aarch64, then CMake found Boost.Fiber but reported that its transitive Boost.Filesystem CMake package was absent from the selected bootstrap subset. The prefix now selects the same eight Boost components as A11. This run does not yet pass the native or wheel gates.

Linux CI run `37199819585` installed the complete Boost closure and configured CMake on x86_64. Its native compile then failed in three Boost-using translation units because the global native `-fno-exceptions` flag also disabled Boost.Context's required `forced_unwind` throw path. CMake now gives `-fexceptions` only to `boost_primitives.cc`, `executor.cc` and `fiber.cc`, following A11's explicit exception boundary. The local compile database confirms those three have `-fno-exceptions -fexceptions` in that order while `select.cc` retains only `-fno-exceptions`; the macOS build and 11/11 CTests passed. Linux retest is pending.

Linux CI run `37200048388` compiled the host C++ on Ubuntu aarch64 through 309 of 311 Ninja steps, then its concurrency test failed to link because `libsymbian_host_primitives.a` followed its required Abseil random archives. The imported host archive now declares those Abseil libraries as its CMake interface dependencies, which places them after the bundle in the generated link command. Local macOS build and 11/11 CTests pass; the Linux native and wheel link gates remain pending.

Linux CI run `37200345002` passed the Ubuntu 24.04 native CMake/Ninja builds and all 10 CTests on both x86_64 and aarch64. The aarch64 manylinux wheel then failed at its final `_native` shared-module link: static libusb contained a non-PIC relocation against `stderr`. The pinned libusb bootstrap now compiles with `-fPIC`, and its cache stamp was bumped. No installed-wheel audit has passed on this revision yet; host native test success is not an SDK export, emulator or device result.

Linux CI run `37200718178` again passed native CMake/Ninja and 10/10 CTests on both Ubuntu architectures. Its aarch64 CPython 3.11 wheel built and auditwheel repaired it, but installation for the wheel audit failed because PySide6 could not be resolved in the manylinux_2_28 aarch64 environment. The package now includes pywebview/PySide6 automatically only on Linux x86_64; aarch64 retains the command-line SDK without a verified Console GUI renderer. The installed-wheel audit remains pending after this packaging change.

2026-10-04 provisional Linux preparation: SDK export now discovers LLVM C/C++/LLD/archive tools from PATH or an explicit `SYMBIAN_LLVM_BIN`, preserving the selected driver filename and C/C++ toolchain pairing. The exported emulator path selects Linux `build/eka2l1/bin/eka2l1_qt`, with `gdb-multiarch` as a guest-debugger fallback. Source Run and IDE launch settings no longer inject Homebrew paths on Linux; macOS-only background-window variables are omitted there. Linux host instructions, source/emulator/debug tabs and a staged validation plan were added. Six focused host-tool/IDE tests passed locally on macOS; Black/Ruff and the strict MkDocs/two-Doxygen build passed. These checks do not establish a Linux SDK export, interactive emulator run, guest GDB session or Nokia 808 compatibility. The first local `cmake --preset debug` exposed a stale concurrency-target reference to the removed `cpp/symbian/concurrency/README.md`. Repointing its CMake `SOURCES` at `.dev/thread-a11.md` restored configure; the host native build then succeeded and all 11 CTest targets passed on macOS. Linux CI remains the next host build check.

2026-10-04 paced documentation revision: Getting started now leads through host tools, a small standalone application, build inspection and local firmware setup. The former long project, firmware, emulator, debug, runtime, Console, device and TLS guides were split into task pages and named capability/reference articles. Four cropped real CLion/IntelliJ screenshots illustrate project source, CMake profile, Run choices and debugger profiles; the stray unsaved IDE edit was excluded from the published crop and reverted in source. The strict MkDocs/two-Doxygen build and local link check pass. GitHub Actions documentation run `37198834084` completed both build and Pages deployment; the live getting-started, project, project-configuration and TLS-reference articles and CLion overview PNG each returned HTTP 200. This is a documentation result, not guest execution evidence.

2026-10-04 connected-socket prerequisite experiment: an extra guest DLL
export opened `socket(AF_INET, SOCK_STREAM, 0)` and tried to attach the
nonblocking Mbed TLS BIO. Both named RM-807 emulator backends exited `-146`:
socket creation succeeded, but `fcntl(F_SETFL, O_NONBLOCK)` failed. Their logs
reported unhandled EKA2L1 ESock base option family 1/id 4, the nonblocking
option. The temporary six-export run finished 3 passed/2 failed; the two
failures were the normal clients on Dynarmic and Dyncom. The experimental
export and test call were removed, preserving the maintained five-export
passing suite. Connected socket callbacks and TLS handshakes remain open;
no physical-device behavior is inferred from this emulator gap.

2026-10-04 IDE guide images: macOS screen capture is now permitted. Four
cropped, actual captures from the prepared `examples/gui_app` project in
IntelliJ IDEA with the CLion plugin show the indexed source/project tree, the
enabled local ARM CMake preset, saved GUI Run/GUI Debug choices and the separate
host LLDB/guest GDB profile selector. The terminal was restored to the
foreground after each capture. The CLion overview, profile and Run/Debug
articles now include these images with role-specific captions. The earlier
SVG remains available but is no longer used as a substitute for IDE captures.
These screenshots establish visible IDE configuration, not a guest run.

2026-10-04 EKA1 planning only: `.dev/eka1-plan.md` scopes a single opt-in,
no-UI process probe on one named EKA1 firmware fixture using current tools
where the ABI permits. It adds no dependency and changes no EKA1 Run behavior;
the existing EKA1 rejection remains the correct preflight result. Application
startup, emulator execution and device compatibility remain open.

2026-10-04 documentation publication: GitHub Actions run `37197271949`
passed its strict MkDocs/two-Doxygen build and Pages deployment. The published
Console and CLion articles, native SDK guide, original-platform Doxygen index
and Console image asset returned HTTP 200. MkDocs uses `.html` article URLs.
The CLion configuration image remains a labeled illustration because the owner
declined Screen Recording access for now.

2026-10-04 entropy link correction: a consumer that pulled the candidate
`mbedtls_hardware_poll` into an E32 DLL failed to link on
`TTrap::Trap(int&)` and `TTrap::UnTrap()`. The pinned EABI EUSER definition
does not export these functions. Enabling the source's exception-based trap
path would require a guest exception runtime and EHABI behavior that this SDK
does not yet verify. The SDK guest archive now links an explicit failure
callback; the `Math::RandomL` adapter remains in source as an unlinked
research candidate. A fresh source export at
`.symbian/mbedtls-safe-sdk-20261004` built and was selected as the active SDK.
Its provenance records 1,727 Mbed TLS source files and all ARM archive
digests; both ARMv5T and ARMv6 crypto archives define the failure callback.
The five-case dynamic E32 DLL suite passed on the fresh SDK, including the
failure callback through `RLibrary` on Dynarmic and Dyncom. The callback
reports `MBEDTLS_ERR_ENTROPY_SOURCE_FAILED` and zero produced bytes.
Secure guest entropy and authenticated guest TLS remain unverified, so
development-agent gate 2 remains open.

2026-10-04 documentation second pass: the former 1,142-line GUI source guide
is now an overview and seven step-oriented articles; the CLion guide is an
overview plus profile, Run/Debug and advanced articles. Two Console screenshots
were rendered from the current frontend in headless Chrome with sample paths
and no phone data; two 720×1280 counter frames came from the retained real
EKA2L1 input test. macOS denied desktop capture from this session, and the
owner chose not to enable Screen Recording now, so the CLion guide uses an
explicitly labeled configuration illustration rather than an IDE screenshot.
The SDK native API guide maps available CMake targets, headers, results and
lifetimes. Doxygen now builds separate SDK and curated original-platform
references; the latter contains nine unedited EPL-noticed SymbianSource header
snapshots and generated 827 HTML pages. The local strict MkDocs and both
Doxygen builds passed. Pages deployment of this pass is pending.

2026-10-04 guest entropy candidate: a new C++ Mbed TLS adapter calls original
`Math::RandomL(TDes8&)` under a Symbian `TRAP`, limits requests to 1024 bytes,
and clears output on a leave. The SDK exporter now includes original
`e32math.h`/`e32math.inl` and adds EUSER source ordinal 2503 to its proxy.
The first export failed because the header's `.inl` include was absent; a
retry at `.symbian/mbedtls-entropy-sdk-retry-20261004` built and was selected
temporarily. Both ARMv5T and ARMv6 crypto archives defined
`mbedtls_hardware_poll` and reference the Math import. No emulator execution,
guest entropy quality/health assessment, or Nokia 808 import check has passed;
the TLS gate remains open.

2026-10-04 documentation icon repair: the MkDocs Material emoji extension now
turns the four overview card icons into inline SVG instead of displaying their
`:material-...:` source text. The strict combined MkDocs/Doxygen build passed;
the generated overview contains four rendered icon spans and no literal
`material-rocket-launch` token. GitHub Actions run `37196505812` passed its
build and deploy jobs; the live overview contains four SVG card icons and no
literal Material icon shortcode.

2026-10-04 TLS transport follow-up: a fresh source export at
`.symbian/mbedtls-socket-sdk-20261004` is selected as the active SDK. It ships
`sdk_socket_bio.c` and its public header in the copied vendor tree and
both ARMv5T and ARMv6 `libmbedtls.a` archives define all four BIO functions.
The expanded dynamic DLL probe imports the BIO object from the three-archive
set and passed five tests on the fresh SDK, including Dynarmic and Dyncom runs
for normal and changed-digest clients. It confirms that a cancelled BIO
returns `MBEDTLS_ERR_NET_CONN_RESET` from send and receive before socket I/O.
It does not exercise a connected guest socket, in-flight cancellation, secure
guest entropy, or a guest TLS handshake. Development-agent gate 2 remains open.

2026-10-04 documentation transition: developer articles now live in lowercase
paths under `doc/docs/`, with MkDocs Material navigation and A11-matched
Noto Sans/JetBrains Mono fonts, indigo light/dark palettes, code highlighting
and navigation settings. Doxygen generates the C++ reference into the same
site. Plans, logs, research notes and other internal Markdown moved under
`.dev/`; the root README is a compact entry point. The local combined build
passed with `doc/build.sh --strict`, including link checks, MkDocs and
Doxygen/Graphviz. The public repository is
`https://github.com/hpnkv/symbian-platform`. GitHub Actions run
`37196098884` passed its build and deploy jobs. The published overview,
credits, source walkthrough and generated Doxygen index at
`https://hpnkv.github.io/symbian-platform/` each returned HTTP 200. The
README and introductory guides now explain E32, SIS, EKA2L1 and Window Server
and visibly credit the external projects. The TLS development-agent gate
remains open at step 2.

2026-10-04 Mbed TLS source ownership and trust packaging: the complete port
working tree (1,721 source/configuration/license files) now lives at
`third_party/mbedtls-symbian`, including its CMake project and required
third-party sources. Default SDK export builds that tree with no sibling
checkout dependency and installs an inspectable copy at
`source/mbedtls-symbian`. The derived, sealed active SDK is
`.symbian/mbedtls-ca-sdk-20261004` (5,302 digested files); its provenance
records the source copy SHA-256
`754a1c1101308adc572d75fa927f7fe9e854a57e940d65919083cdf835d75695`
and all six rebuilt archive hashes. Independent C consumers configured and
built on ARMv5T and ARMv6 against this SDK's architecture-matched
`MbedTLS::mbedtls` target; a plain target had no TLS link. The vendored host
suite passed 10/10, including authenticated TLS 1.2/1.3 application-data,
wrong-hostname rejection and an invalid-UTC adapter case, using host entropy
and time. The guest archive now defines `symbian_mbedtls_time` and
`mbedtls_platform_gmtime_r` through the SDK clock/libc imports. A dynamic DLL
probe on both emulator backends converted 2024-01-01 UTC, rejected a pre-2020
date and observed a clock above the plausibility floor; physical clock
correctness remains unverified.

`SYMBIAN_CA_BUNDLE` now selects only a project-local PEM file. CMake rejects
outside/missing or oversized files and exposes the packaged path; packaging
validates each certificate, rejects other PEM content, embeds the selected
bundle as an application resource, and records its SHA-256. An unset option
includes no roots. A rebuilt native boundary generated and re-inspected a
four-file SIS with the exact PEM bytes; the focused native SIS case and two
Python CA tests passed. SDK-wide trust was not changed. The historical
`rand()` guest entropy example was replaced by an explicit failure result.

The guest SHA-256 DLL test now passes five cases against the active SDK: the
declared export is retained while unused mimalloc POSIX helpers are discarded,
and a dynamic `RLibrary` client verifies the `abc` digest and changed-input
failure on Dynarmic and Dyncom. Its E32 import table lists EUSER, libc and
libpthread. The expanded guest DLL also parses an explicit test PEM trust
root, accepts its certificate for `sdk-test`, and rejects `wrong-name` and an
expired certificate on both CPU backends. A fresh active SDK at
`.symbian/mbedtls-transport-sdk-final-20261004` includes source-ordinal libc proxy
entries for these crypto dependencies and the selected socket functions.
Those socket symbols have not been exercised in the emulator. No guest TLS
handshake, verified guest entropy or physical UTC, cancellable socket owner,
emulator TLS memory
bound, or Nokia 808 connection is claimed. DEVELOPMENT_AGENT step 2 remains
open; steps 3 and later have not started. An ARM archive or host handshake
does not establish guest loader or Nokia 808 compatibility.

A new Mbed TLS socket BIO adapter makes an attached descriptor nonblocking,
returns WANT_READ/WANT_WRITE for retryable calls, and exposes atomic
cancellation that stops later callbacks. The host suite passed 12/12 with
read/write, empty-read and cancellation checks. This code was added after the
latest SDK source export; guest socket execution and SDK distribution of this
adapter require a fresh export and remain unverified.

2026-10-04 Mbed TLS SDK integration: the default exporter now builds the
local Mbed TLS 3.4.1 port for ARMv5T and ARMv6, installs architecture-specific
static CMake packages plus public headers and Apache-2.0 notice, and records
archive digests/source state in SDK provenance. A staged SDK copy was populated
from fresh Release builds: both architectures compiled all three archives and
configured/built a consumer that found the matching `MbedTLS::mbedtls` target.
The exporter helper itself subsequently rebuilt and installed both packages
in `.symbian/mbedtls-export-helper-test-20261004`; its SDK manifest loaded,
and an independent consumer compiled both targets on each architecture. The
consumer's `plain` target had no Mbed TLS dependency. This is compile
and packaging evidence; guest TLS 1.2/1.3 handshakes, entropy/time services,
certificate validation and physical-phone connections remain unverified.
The complete from-source install then passed at
`.symbian/mbedtls-default-sdk-20261004` and became the active SDK. Its
manifest, headers, license, provenance and six archive digests verified;
independent consumers compiled against both architecture packages.
An ordinary SDK copy/install also passed; a consumer found and compiled
against the relocated ARMv6 package. The from-source installation remains
selected as the active SDK.
An SDK-preparation simulation without compiler wrappers confirmed that the
exporter must pass both matching LLVM compilers; after that correction, all
three ARMv6 archives built with Clang 23.1.2 for C and C++.
Cloning a pre-TLS SDK now fails with a clear `FAILED_PRECONDITION`, requiring a
source export instead of silently producing another incomplete default bundle.

2026-10-04 emulator controls: SDK-owned macOS background emulator windows now
place the complete emulator menu bar inside the window, covering IDE Run/Debug
and automated GUI sessions. Foreground launches retain the macOS application
menu. The maintained EKA2L1 patch reverses cleanly, the patched `eka2l1_qt`
target rebuilt, and the live RM-807 GUI render/input/exit test passed on
Dynarmic and Dyncom (2/2). A live 896-by-754 window was observed through
CoreGraphics; macOS denied window image capture from this shell, so visual
confirmation of the menu remains open.

The initial technical survey is in RESEARCH.md. The owner has one Nokia 808;
its AT modem reports RM-807 and firmware revision 113.010.1508, while product
code, independent firmware verification and recovery method remain unknown.
The supplied Delight RM-807 archive provides preserved emulator ROM/Z material;
guarded disposable tests now verify real GUI pixels, pointer-driven redraws,
reset and normal zero guest/frontend exit on both macOS CPU backends.

Latest 2026-10-02 development checkpoint:

* `symbian console` now starts a wxWidgets frontend using native desktop
  controls on macOS and Windows. It has six purpose-based sidebar sections,
  rounded grouped content surfaces and higher-contrast guidance; guided views
  for all 38 public CLI workflows, USB inventory and bounded AT/MTP/PC Suite
  OBEX probes. Its FastAPI/httpx API remains in-process with no network
  listener. A live macOS wx event-loop check loaded all 38 actions and the
  connected 808; long forms measured a 903-pixel scroll extent inside a
  590-pixel viewport. The 57 focused console/CLI/device tests passed, as did
  a clean-installed macOS ARM64 wheel audit with wxPython 4.2.4. A live
  `symbian console` invocation returned in 0.12 seconds and left the wx GUI
  running. No new physical-device protocol run was claimed from this GUI
  check; see [CONSOLE.md](../doc/docs/guides/console.md).

* The native console status bar now shows a green bullet, phone name, USB IDs
  and interface profile only while a selected phone is observed. An idle
  disconnect clears the device field. Active device work and a saved USB mode
  baseline show amber pending status through temporary disconnection; a
  failed request is marked unverified until the next inventory result. Three
  state tests brought the focused console/CLI/device suite to 60 passing.

* `device info` now presents the connected 808's interfaces by USB-standard
  role and device-declared name, with endpoint counts, alternate settings,
  host driver and exact serial-port association. Its human view uses terminal
  color while redirected output remains plain; JSON retains the numeric
  descriptors. Live IOService names include MTP, PC Suite Services, SyncML,
  Haptics Bridge, UsbPnComm and LCIF. Interface 0 is still-imaging/PTP class
  labeled `MTP`; interfaces 1–2 are CDC ACM control/data, with the observed
  AT port under interface 2. The named PC Suite OBEX pair was later validated by a Connect/Disconnect
  exchange; other named channels remain descriptor evidence.
  Old mode tickets compare only stable numeric fields, so descriptive metadata
  does not manufacture a USB transition.

* `device info` now issues bounded read-only `AT`, `+GCAP`, `+CGMI`, `+CGMM`
  and `+CGMR` on the observed 808 PC Suite USB candidate's CDC ACM port;
  `--no-protocol` omits this probe. The physical phone returned `OK`,
  `+GCAP: +CGSM,+DS,+W`, Nokia 808 PureView, and revision
  `113.010.1508 2013-01-02 RM-807 (c) Nokia`. The CLI labels the parsed
  revision/date/RM as AT-reported identity. This establishes basic modem
  commands on that interface, not PC Suite protocol, OS build, phone logs or
  debugger access. The probe has fixed commands, one-second exchange limits,
  a 4 KiB response bound, and suppresses serial-like identity responses.

* Physical USB inspection now exposes exact interface descriptors, a
  serial-derived mode-stable anchor, and host serial-port paths. The owner
  switched the connected 808 from mass storage to PC Suite mode. IOService
  then showed `0421:05d1` at the same location, 18 interfaces, no mounted
  volume, and `/dev/cu.usbmodem141202`, versus prior `0421:05d0` and one
  `08/06/50` interface. The new `device mode begin/verify` workflow records a
  private baseline and checks same-serial re-enumeration; it cannot verify
  PC Suite protocol access or device debugging without a handshake. Ten
  device Pytests pass, including changed-product, impostor and Linux
  descriptor controls. A live begin/verify check correctly returned
  `unchanged`; a real ticketed transition was not captured because the
  handset was switched before the command existed.

* A fresh mimalloc-default ARMv6 `gui_app` compiled reproducibly and packaged
  with application registration and localized resources. The 17-case native
  GUI package verifier passed. The unsigned SIS SHA-256 is
  `be5ab91b933af743113dd1bba74c3edb626129cbcc04427ba3aec4a0d86ddcad`.
  One connected Nokia `808 PureView` USB descriptor had a USB-ancestor-linked
  writable FAT32 volume. The SIS copied and hash-verified at
  `Installs/gui_app-be5ab91b933a.sis`, then `disk4` ejected cleanly. The
  state is `awaiting-on-device-install`: no on-phone installer approval or
  application launch was observed. RM code, firmware and OS remain unknown.

* Mimalloc v3.5.3 is now the default guest allocator in both the standard and
  streams runtime archives of a fresh local SDK. Its pinned headers and MIT
  license ship with the SDK; the original allocator remains available by
  configuring `SYMBIAN_RUNTIME_MIMALLOC=OFF`. The separate native 64-bit
  atomic archive still uses that original allocator because the mimalloc port
  uses the portable atomic lock for generic operations and the native EUSER
  exports are not verified for every ROM. The mimalloc backend builds from pinned ignored
  sources with disconnected `RChunk` reserve/commit/decommit and pthread
  thread cleanup. The 8 MiB virtual arena option commits pages on demand; a
  cross-thread diagnostic held about 580 KiB after collection and two 512 KiB
  reuse bursts stayed within the probe's backing limits. A default 64 MiB
  total virtual reservation budget bounds this optional phone profile, and
  the guest checks its failure path. ARMv5T/ARMv6 ×
  Dyncom/Dynarmic functional guest runs passed 4/4. A warmed allocation burst
  was slower than the safe default heap on both CPU backends (Dyncom
  12,048 versus 1,595 fast-counter ticks; Dynarmic 12,058 versus 380).
  Follow-up instrumentation found only two `RChunk` reserves and three commits across
  the warmed burst, but 33,027 pthread TLS lookups. A single-thread cache
  ablation cut the mimalloc burst to 629 ticks; it was removed because it
  cannot serve multiple worker threads. Imported pthread TLS lookup is the
  measured bottleneck in this emulator profile. A fixed 256-slot native-ID
  cache now serves explicitly entered threads; collisions and unregistered
  threads use the original pthread keys. Writes always update pthread TLS.
  Main and guest worker threads have paired entry/exit; raw `RThread` stays
  uncached because this firmware skips its pthread exit destructor. The safe
  warmed burst measured 817 ticks on Dyncom and 116 on Dynarmic, versus the
  default heap's 1,595 and 380. The optional source profile passed the full
  ARMv5T/ARMv6 guest probe on both backends, including raw and managed thread
  exit, cross-thread free and bounded reuse. A fresh ignored installed SDK
  candidate linked the event executor and passed ARMv6/Dyncom and Dynarmic.
  A combined mimalloc stream-runtime and installed guest fiber image passed
  the full event, worker, native-thread and base allocation probe on both
  backends. That integration exposed and fixed tiny C++ `new` alignment.
  The new installed SDK passed the standard and event/worker guest probes on
  ARMv5T/ARMv6 and Dyncom/Dynarmic (8/8), including direct `<mimalloc.h>`
  calls, cross-thread free and managed cache cleanup. GUI rendering and input
  passed on both backends (2/2); copied-SDK and moved-project builds passed.
  Its 3,148 payload digests match. Emulator ticks establish neither phone nor
  application latency, and the memory-pressure comparison remains open.

* Cross-thread C++ destruction now frees through the allocating Symbian
  `RHeap`. Each allocation holds an `RAllocator::Open` lease until its direct
  `Free`/`Close`, so the producer may still be running or may already have
  exited. The process-owned `RChunk` page bridge likewise pins the heap that
  holds its handle metadata. An explicit private-heap `RThread` probe checks
  both directions, freeing while the producer remains alive, page close after
  producer exit, and heap-cell balance. Its normal/changed controls passed
  8/8 across ARMv5T/ARMv6 and Dyncom/Dynarmic; the event executor matrix
  passed 8/8 on the same candidate. The 3,135-file SDK digest audit, GUI
  link, generated-project SDK copy/relocation and root CTest 10/10 passed.
  The candidate remains local; no physical-device or allocator latency claim
  follows from these functional tests. The per-allocation lease and native
  heap lock still have a cost. This bridge remains the explicit
  `SYMBIAN_RUNTIME_MIMALLOC=OFF` compatibility profile.

* Guest callers can now opt into worker placement. `ThenOnWorker` posts a
  stackless transform and copies the ready result on the worker; inline
  `Then`/`OnReady` semantics remain unchanged. `PostFiber` creates a fiber
  on that worker's own scheduler. `EventExecutor::workers()` lazily provides
  one shared worker, and direct `WorkerExecutor` construction remains possible.
  The cap covers queued and active work; close is nonblocking and `Finish`
  reports drainage. Installed-SDK guest normal/changed controls passed 8/8
  across both ARM targets and emulator backends, including worker affinity,
  fiber sleep, queue saturation and asynchronous finish. The GUI linked and
  generated-project copy/relocation passed. No callback is preempted; a
  non-yielding worker task can delay other worker work, and an uncompleted
  fiber can keep `Finish` pending. This is an explicit guest placement API,
  not yet A11's shared `Post`/`PostAt` pool.
  The guest-only `future.ThenOnWorker(event_executor, transform)` member now
  selects the event owner's lazy worker; a closed executor returns a failed
  Future. The final installed-SDK candidate passed its 8/8 guest matrix,
  has 3,135 verified payload files, and passed GUI link, generated-project
  relocation, root CTest 10/10 and the pinned host source/test checks.

* Guest `thread::Case`, `PermanentEvent`, `Select` and `SelectUntil` now
  compile into `Symbian::Fibers`. A fresh installed-SDK candidate passed the
  event executor probe 8/8 across ARMv5T/ARMv6, Dyncom/Dynarmic and
  normal/changed controls. A further 8/8 run asserted event-OS-thread
  affinity for the property continuation, completion callback and waiting
  fiber. The probe includes immediate and timed selection,
  worker notification, competing events and repeated timeout cleanup. The
  GUI linked against the candidate, and the generated-project initial-build,
  copied-SDK and moved-project check passed. Continuations run on the event
  OS thread without a worker context switch; fiber ready snapshots now
  reuse storage across yields. There is no enforced short-work limit:
  compute-bound callbacks can indefinitely delay native service, despite the
  per-source count budget. Selectable channel cases, rendezvous, cancellation
  cases and full A11 tests remain open. The candidate is not yet the visible
  SDK, and these are emulator results only. Its 3,133 payload digests verify;
  root CTest passed 10/10 and the three original A11 host executables passed.

* The first shared guest event owner now dispatches RTimer, real
  RProperty::Subscribe results, the bounded event mailbox and ready fibers
  before its sole native request wait. Fiber sleep arms an RTimer through that
  owner; event-affinity dispatch has an explicit operation. A structured
  timer/property owner forwards cancellation and an absolute deadline, then
  completes its asynchronous join after child and deadline-alarm drainage.
  The GUI and generated timer-task starter use the event owner. An
  installed-SDK candidate exported both headers, and both applications
  compiled against it. The GUI's rendered input/reset/normal-exit test passed
  on Dyncom and Dynarmic.
  A dedicated guest normal/changed-result probe passed 8/8 across ARMv5T/
  ARMv6 and both backends; it checked a real property value, timer, fiber
  Await/sleep, cancellation, repeated asynchronous TaskGroup Finish, the
  structured owner's success/close/timeout/error and destruction-before-drain
  paths, cross-thread event dispatch and heap-cell balance. This remains a
  bounded event executor, not full A11 `thread::` parity: native request
  ownership is still split between the existing adapters, selectable channel
  cases, A11 fiber trees/pools are open, and no phone result is claimed.

* The SDK exception policy now follows A11: native targets default to
  `-fno-exceptions`, with explicitly selected implementation translation units
  allowed to enable exceptions. The pinned, unmodified full A11 host
  `cpp/thread` compiled with that boundary, and its three original host test
  executables passed in an isolated build. The maintained
  `full_host_probe/CMakeLists.txt` reproduces this check. The staged SDK host
  archive still needs a deliberate replacement; this result does not claim
  that all A11 host APIs ship in the installed SDK.

* Host `thread::` now includes an A11-derived custom ready-fiber scheduler,
  idle-park guard, the original MPMC work queue, shared stackless `Post`/
  `PostAt`, and original `PermanentEvent`/`Select` with wait diagnostics still
  omitted. The Python boundary installs A11's CPython GIL park pair and
  deferred-reference discipline; a bounded Future-to-asyncio bridge resolves
  on the captured event loop. Native tests passed policy ordering, lock/GIL
  park balance, pool callback/timer and Select controls; an extracted macOS
  wheel passed concurrent Python progress, asyncio completion/cancellation and
  deferred-reference drainage. The installed SDK's Boost-free host consumer
  passed its expanded Post/Select test. A11 fiber trees, pool work stealing,
  Submit/Schedule, channel selection, introspection and complete shutdown
  remain open, so this is not a compatible full A11 host port.

* Repository and generated-project C++ formatting now uses clang-format 23's
  `InsertBraces` rule. The starter contains its own `.clang-format`; 170
  first-party C++ files passed a formatter idempotence check after 47 owned
  files were updated. The two user-owned edits were preserved. The selected
  visible SDK was refreshed with the generator, formatted starter and CMake
  target closure; 3,123 payload digests verify. Its initial-build, copied-SDK
  and moved-project test passed. Root CTest passed 10/10, and the built macOS
  wheel contains the template dotfile. The initial installed starter build
  exposed a missing guest fiber archive link from `Symbian::Stackless`; the
  target now supplies that archive when installed.

* A bounded guest `thread::Fiber` now runs on the verified ARM/Thumb switch,
  pinned to one OS thread and a 16 KiB stack. `thread::Mutex` contention and
  `thread::CondVar` signal/timeout waits park a fiber; `thread::SleepFor` and
  an unresolved `Future::Await` do likewise. Outside a fiber, unresolved
  guest Await fails clearly. `thread::SchedulerPolicy` provides custom
  ready ordering and a cross-thread wake callback outside internal locks.
  A normal/changed control covering move-only work, lock contention,
  cross-thread signal, timeout, LIFO policy, Future await and heap balance
  passed 8/8 on ARMv5T/ARMv6 × Dyncom/Dynarmic. A fresh SDK export builds
  a separate `Symbian::Fibers` archive for each architecture; installed-archive
  execution passed 8/8, and the updated visible SDK passed the same 8/8
  matrix against its existing Abseil archives. Event-owner integration,
  A11 Select/pool/tree,
  cancellation and joining remain open.

* Host and guest now share one portable A11-derived channel, Future/Task,
  TaskGroup and mailbox source layer. The host CMake target
  `symbian::concurrency` selects an opaque `thread::` primitive ABI; its
  Boost.Fiber/Context implementation is bundled inside one SDK static
  archive. An ordinary consumer has no Boost headers or separate Boost link
  dependency; Boost is needed only to rebuild that archive. The host's bounded
  `thread::Fiber` handles move-only
  work, same-thread join, cooperative cancellation and normal C++ cleanup;
  call sites use `thread::Fiber`, not Boost types. The SDK export includes the
  host archive and Boost-free headers. An installed host consumer compiled,
  linked and ran with Boost discovery disabled; its link command named no
  Boost library. Root CTest passed 10/10. An
  earlier no-Boost fallback test passed before that incomplete branch was
  removed. The updated installed-SDK
  guest stackless/channel/timer matrix passed 16/16 across ARMv5T/ARMv6 and
  Dyncom/Dynarmic. Guest `CondVar` now matches A11: true means timeout.
  A fresh workspace export passed four guest timer/Future cases and an SDK
  copy/build check. That earlier export had 3,113 verified payload digests.
  The new visible SDK verifies 3,122 payload digests; its prior tree is
  preserved at `~/dev/symbian-sdk-before-fibers-20261002`.
  A11 tree/Select/pool semantics and comprehensive guest fiber lifetime remain open.

* Guest synchronization and channel headers now keep A11's `thread::`
  namespace: `<thread/boost_primitives.h>` and `<thread/channel.h>`.
  The shared channel backend supports bounded FIFO `Channel<T>` with
  `reader()`/`writer()`, move-only values, blocking worker reads/writes,
  nonblocking Abseil-status operations, close wakeups and drainage.
  `EventMailbox` uses the nonblocking channel path. Original pinned libc++
  condition-variable destructor support was added to both runtime archives.
  A timed-wait control exposed an early `no_timeout` return without a signal;
  the adapter now validates signal generation and uses monotonic bounded
  waiting. The installed normal/changed guest stackless and timer/channel
  matrix passed 16/16 on ARMv5T/ARMv6 and Dyncom/Dynarmic. The fresh visible
  SDK had 3,113 verified payload files after the CLI refresh; the prior
  3,110-file SDK is preserved at
  `~/dev/symbian-sdk-before-channel-20261002`. Full A11 `Select`, fiber lifecycle,
  zero-capacity rendezvous and exception-throwing closed writes remain open.

* CLI inspection and workflow commands now default to human-readable
  summaries, with TTY-aware color and readable byte sizes. `--output-format=json`
  preserves the canonical schema for scripts before or after nested commands.
  Root and nested help describe every command and option. Long result lists
  use separate numbered lines. The live `device list` output showed the
  connected Nokia USB and mounted storage without its serial;
  44 CLI/device/control/SDK tests and the selected-SDK project regression
  passed. A rebuilt, separately installed macOS wheel passed its resource,
  native-dependency and CLI audit. The unrelated older package fixture still
  fails at the ARM exception-descriptor gate before CLI output is reached.

* Generated applications select an SDK through an ignored local
  `sdk-location.json` containing only a path, with no SDK digest lock.
  A two-way switch between the visible SDK and a second installation passed
  through `symbian app build`: compiler identity and
  `compile_commands.json` followed each selection. A direct CMake/Ninja
  control initially did no work after the path changed; generated
  `sdk.cmake` now declares the location and preference files as configure
  dependencies. Direct builds then reconfigured, rebuilt and selected the
  new compiler in both directions. The GUI example's 19 KiB source/digest
  manifest moved from its application folder to
  `research/gui_app/source-profile.json`, where SDK source preparation
  actually needs it. Synthetic staging checks passed 12/12; preparation
  against the pinned original source tree succeeded, and the current
  six-DLL GUI link test passed. The visible SDK template was refreshed and
  its digest manifest resealed. The generated-project SDK-switch regression
  passed with the prepared visible SDK (1/1). A full prepared-workspace
  export using the moved profile succeeded and all 3,109 payload digests
  verified; the visible SDK remained the global default.

* Localized menu resources and project SVG icons now build through ordinary
  `[application]` settings. The host policy maps BCP 47 tags to selected
  `TLanguage` IDs and invokes original `rcomp` with both Unicode output and
  UTF-8 source interpretation. Native SIS writing/inspection accepts a bounded,
  canonical resource set and converts an SVG to a same-host deterministic gzip-backed
  MIF. The maintained `gui_app` packages fallback/French/German/Japanese
  captions and one icon as seven files. On both CPU backends, EKA2L1's original
  installer installed/reloaded/removed them, AppArc selected all three
  translations and decoded `Zähler` and `カウンター`, and its original MIF
  reader recovered the SVG
  (8/8 headless tests). The separate native SIS suite passed 9/9 and the
  resource Pytests passed 2/2. These headless cases execute no guest
  instructions; physical Belle SVG-in-MIF rendering has not been observed.
  The physical `menu_v5` observation below predates this icon/locale change.
  A fresh 3,109-file SDK export passed digest audit and its own ARMv6
  init/build/package smoke. After promotion, the visible SDK itself completed
  ARMv6 and ARMv5T init/build/package checks; its ARMv6 sample included
  French and Japanese menu translations. The usual `~/dev/symbian-sdk` path
  was updated from that tested export and rebased; the previous tree remains
  at `~/dev/symbian-sdk-before-icon-locale-20261002`. The macOS wheel's
  installed dependency/resource audit also passed with the icon template
  and system zlib dependency.

* Application-menu packaging now uses the original EPL-licensed Symbian
  `rcomp` at pinned revision `d3c2eadd`, with a replayable 64-bit host patch
  and an SDK UID-checksum companion. A prepared-workspace SDK export contains
  both tools, genuine `AppInfo.rh`, the EPL notice and 3,108 digest-valid
  files. `[application]` gives `caption` and `short_caption` directly in
  `symbian.toml`; `symbian init` generates defaults, and `gui_app` specifies
  its own. Native SIS writing/inspection verifies the EXE, registration and
  caption resources, their hashes, UIDs and same-drive install paths.
  Eight native SIS tests, a real-resource Pytest, fresh ARMv5T and ARMv6
  init/build/package checks and `gui_app` packaging passed. An original
  EKA2L1 AppArc parser check exposed that `rcomp` needed its `-u` Unicode
  mode: the earlier byte-text SIS installed but did not decode its menu
  captions. The corrected resources parsed as `Counter` and `Symbian GUI
  Counter` on both backends. The older E32-probe packaging
  fixture still fails at its separate exception-descriptor gate.
  The 3,108-file export was rebased into `~/dev/symbian-sdk`, with the
  prior digest-valid tree retained at
  `~/dev/symbian-sdk-before-menu-registration-20261002`. The visible SDK
  repackaged `gui_app` with Unicode resources. EKA2L1's original installer
  accepted all three files, parsed their menu text, reloaded the package registry and removed and
  reinstalled the resources on Dyncom/Dynarmic (8/8 headless tests).
  The visible SDK's full GUI package verifier passed 17 image/checksum and
  installer cases with the current six-DLL/153-slot executable; it still
  records zero executed guest instructions. The separate on-phone result below
  is user-reported, not inferred from that verifier.

* Physical-device groundwork now has `symbian device list`, `info` and a
  build/package/USB-mass-storage staging flow. IOService associated the
  connected `808 PureView` USB descriptor with a writable mounted S60 disk;
  the Mac USB system profiler alone had returned no devices. The public
  selector hashes the serial, and RM code, firmware and phone runtime remain
  unknown. Eight synthetic discovery/transfer/policy/Linux/low-space controls
  passed. A real generated app in `~/dev/symbian-app-3` rebuilt reproducibly,
  and its unsigned SIS passed a host-only staging/hash check. After the owner
  confirmed the photos were copied, an ARMv5T portable generated starter
  with Unicode menu resources was copied to the phone's `Installs` folder.
  The copied SIS SHA-256 is
  `db4875221d6ecabbe02bfba76c4201c5da5f8f0df298ae50f7f5050356e70db1`;
  `disk4` ejected normally. The owner then reported that the handset installer
  succeeded, `menu_v5` appeared in the application menu, and the app opened
  and responded to a tap. This is direct user observation, not an SDK-read
  installer registry, captured device log or instrumented launch trace. A
  direct installer transport and real Linux-connected phone remain untested.
  `examples/gui_app`'s standalone reproducibility check still returns
  `DATA_LOSS` for differing CMake ELF/E32 builds, an independent open issue.

* `examples/gui_app` now links the installed `Symbian::Stackless`/guest Abseil
  profile through a narrow modern C++ bridge. A 300-ms timer Future lights a
  separate marker after an increment; Reset cancels pending work. The existing
  counter, drawing and owner's `app.cc` edits remain intact. The same event
  thread consumes Window Server and timer completions. The final candidate
  built reproducibly, both root and standalone CMake targets linked, and the
  expanded pixel/marker/cancellation/exit GUI control passed on ARMv5T and
  ARMv6 × Dynarmic and Dyncom (4/4). The root `gui_app_e32` publisher also
  rebuilt under Apple Clang, and its ARMv6 image passed both backends after
  SDK promotion. `symbian init` now defaults to Abseil Status/StatusOr and
  timer-backed Tasks, with a portable option and automatic portable selection
  when `--firmware` names a Z drive without `libpthread.dll`. A final-candidate
  CLI initial-build, relocated SDK/project and two-backend Task/cancellation/
  rapid-exit control passed on ARMv5T and ARMv6 × Dynarmic and Dyncom (4/4);
  the full generated-project suite passed 17/17 before the additional ARMv5T
  cases passed 2/2. A preceding candidate passed real starter
  builds and GUI execution on C7, E6, 6120 and E71 (4/4); the latter two used
  the portable profile. A deliberately modern executable against E71 now
  returns canonical `FAILED_PRECONDITION` before emulator startup. Root CTest
  passed 8/8. The digest-valid 3,100-file candidate was rebased into the
  visible `~/dev/symbian-sdk`; the prior 3,100-file tree remains at
  `~/dev/symbian-sdk-before-gui-init-migration-20261002`, also digest-valid.
  The promoted SDK completed a new default `symbian init` build. Its GUI
  publisher passed again on both ARM targets and both emulator backends.
  These are application-path results; A11 fibers and genuine
  `thread::` primitives remain open.

* The first C3 backend prerequisite now lives in the runtime archive, with an
  ARM/Thumb context swap preserving callee-saved registers across a bounded
  16 KiB heap stack. A C++ probe retains heap-backed string/unique ownership
  across two switches, destroys both normally and checks heap-cell balance.
  Installed normal/changed execution passed 8/8 on ARMv5T/ARMv6 ×
  Dyncom/Dynarmic. This is raw context switching only: stack guards,
  floating-point context, native TRAP safety, fiber scheduling, joining and
  genuine A11 `thread::Mutex`/`CondVar`/`PermanentEvent`/`SleepFor` remain
  unimplemented. The sealed candidate was promoted into the visible SDK;
  ARMv6/Dynarmic normal/changed controls passed again after promotion. The
  prior sealed installation is preserved at
  `~/dev/symbian-sdk-before-fiber-swap-20261002`.

* A second native request owner now runs `RProperty::Subscribe` through a
  typed `Future<int>` with SDK-owned status, result storage, handle, native
  cancellation and close-time drainage. It shares the timer pump's sole
  event-thread semaphore consumer. An `EventMailbox` bounds cross-thread
  dispatch and executes callbacks outside its lock; the generated timer GUI
  uses it. The installed candidate passed 8/8 timer/property/mailbox normal
  and changed controls on ARMv5T/ARMv6 × Dyncom/Dynarmic. The generated GUI
  timer and Status controls passed 5/5 against the candidate and 6/6 including
  a copied Abseil project against the 3,100-file promoted visible SDK.
  Absolute timer deadlines now take real-world `absl::Time`, convert once at
  registration and wait monotonically; a simulated post-registration wall
  jump, 24-hour admission/cancellation, infinity and native slice boundaries
  passed. Actual wall-clock adjustment and full-length rearm are not tested.
  A subsequent export adds an `Await` guard: ready results work and an
  unresolved `Await` returns `FailedPrecondition`; its stackless matrix passed
  8/8 and the visible SDK was updated. Its ARMv6/Dynarmic normal/changed
  cases passed again after promotion. A post-promotion timer/property matrix
  passed 8/8 with an added pending
  subscription-close/drain control. Full A11 `thread::` mutex,
  condition variable, permanent event and sleep semantics require the C3
  fiber backend; no OS-thread-only lookalike is published.

* A sealed development SDK candidate at
  `.symbian/abseil-direct-status-sdk-20261002` now publishes the pinned guest
  Abseil types directly through `symbian::concurrency`: Future/Promise/Task
  results are `absl::StatusOr<T>`, failures are `absl::Status`, and the earlier
  name-shortening aliases are gone. Installed-SDK stackless and timer controls
  each passed 8/8 on ARMv5T/ARMv6 × Dyncom/Dynarmic, including changed-result
  exits. At that checkpoint `TimerPump` used `absl::Duration` with a
  provisional monotonic-deadline wrapper; the later export above changed
  absolute deadlines to `absl::Time`. The
  Status-enabled generated GUI passed both backends, and its injected model
  failure reached the native exit path (3 tests). The pinned Abseil
  Status/StatusOr/Cord/map/time installed contract passed 9/9. Cross-thread
  closing of an SDK-owned page source and the original Abseil allocator passed
  normal/changed controls on the preceding candidate. Root ARM IDE CMake now
  has explicit targets for both Abseil probes; ARMv5T/ARMv6 configure and
  object compilation passed. General A11 scheduler/fibers, OS TLS and broader
  Abseil remain open. That candidate was promoted later as recorded above.

* Genuine A11-pinned guest Abseil `Status`, `StatusOr`, `Cord` payloads and
  `flat_hash_map<std::string, int>` now execute on RM-807. The normal and
  deliberately changed-result contract passed 8/8 from replayed source and
  8/8 through the installed `Symbian::AbseilStatusOr` target on
  ARMv5T/ARMv6 × Dyncom/Dynarmic. The visible SDK contains 384 Abseil
  headers and 43 compiled closure archives per architecture, plus exact source
  revision, three replayable patch digests, Apache-2.0 license and imported
  CMake target. A copied project built and converted to E32 using only the
  installed SDK for target inputs. The 3,097-file export is installed at
  `~/dev/symbian-sdk`; its verified 2,597-file predecessor is retained at
  `~/dev/symbian-sdk-before-guest-abseil-20261002`. The wider selected
  runtime regression passed 24 cases with one skip. The stream profile now
  supports the
  necessary 16-bit-wide libc++ surface and selected real OpenC wide/stdio
  functions; SDK adapters provide `nan`, `nanf` and `ldexpl` where no frozen
  export exists. Byte and bitwise atomics use real EUSER operations, while
  Abseil's table-seed TLS is adapted to a process-wide atomic sequence.
  This is a tested Status/StatusOr/map subset, not every Abseil component,
  general ELF TLS, cross-thread page release or A11 fibers.

* The pinned A11 Abseil `LowLevelAlloc` now links from only two ordered,
  replayable source patches and executes on the RM-807 Dynarmic guest. Its
  original allocator and arena code allocate a 130,000-byte block through an
  SDK-owned process `RChunk` page source; an arena with an outstanding block
  refuses deletion, then closes after that block is freed. A maintained
  opt-in guest test passed the normal and deliberately changed-result cases
  (2/2) against a fresh 2,597-file SDK export and again against the visible
  installed SDK. Both patches apply to the
  pristine A11 pin and reverse-check from the replay checkout. Runtime exits
  now use named internal reasons with compile-time `KErrNoMemory` and
  `KErrArgument` checks. The related installed-SDK runtime exit/clock/lifecycle
  selection passed 53 guest cases; root CTest passed 8/8. The 2,597-file SDK
  was promoted to `~/dev/symbian-sdk`, with the verified 2,596-file predecessor
  at `~/dev/symbian-sdk-before-closure-final-20261002`. A copied-project
  test passed before and after promotion. Cross-thread page release, memory
  pressure and A11 fibers remained open at that checkpoint; the newer
  Status/StatusOr result is recorded above.

* The runtime now has an owned `RChunk` page bridge with validated page size,
  page-aligned process-owned allocation, explicit handle release and a heap-cell
  balance check. ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed controls
  passed, as did
  a separate `pthread` TLS-key test for worker isolation and its exit
  destructor. EUSER byte exchange and OpenC `strtol`, `strcpy`, `strcmp`,
  `sysconf` execution controls passed on ARMv6/Dynarmic. This is page sourcing
  and selected services, not Abseil LowLevelAlloc or general ELF TLS. A
  replayable Abseil patch makes `GetCachedTID()` use its existing
  `pthread_self()` fallback on Symbian; the isolated relink no longer reports
  `__tls_get_addr`. Other allocator, synchronization and C-service undefined
  symbols remained at that checkpoint. The selected 2,597-file export
  passed its allocator test and was promoted as recorded above.

* LLVM's original ARM soft-double compiler-rt implementation now runs under
  the SDK's ARMv5T and ARMv6 profiles. An ordered, replayable LLVM patch
  changes four ARMv6T2-only `movw` constant loads to literal loads and one
  `bfc` to ARMv5-safe shifts; original helper sources complete its dependency
  closure. Actual guest double arithmetic, comparisons, NaN and conversions
  passed 8/8 normal/changed
  source-built and 8/8 installed-candidate ARMv5T/ARMv6 × Dyncom/Dynarmic
  controls. The candidate's copied-project test and root CTest 8/8 passed.
  The 2,596-file SDK was promoted; its verified predecessor is retained at
  `~/dev/symbian-sdk-before-softfloat-20261002`. This removes all floating-point
  compiler ABI undefined symbols from the isolated pinned Abseil link, but
  allocator, TLS, synchronization and C/POSIX services still block genuine
  guest Status/StatusOr execution.

* An integration control now uses A11-derived `TaskGroup` over real
  `TimerPump` requests: joining two timers succeeds only after native
  completion; cancelling an aggregate of three timers forwards cancellation
  to each request and settles after event-thread drainage. ARMv6/Dynarmic
  normal and changed-result source cases passed (2/2), followed by all 8/8
  ARMv5T/ARMv6 × Dyncom/Dynarmic source controls and 8/8 installed-SDK
  controls. This is stackless structured composition for timers, not a fiber
  backend.

* `TimerPump` now limits simultaneously pending native timer Tasks (64 by
  default, configurable). Saturation returns a ready Task with
  `kResourceExhausted` before allocating an OS timer; a slot is reused after
  event-thread completion. A two-slot saturation/reuse/close and heap-cell
  control passed 8/8 source-built and 8/8 installed-candidate
  ARMv5T/ARMv6 × Dyncom/Dynarmic normal and changed-result executions.
  The candidate contains 2,596 digest-verified files. This bounds one native
  request type, not all A11 continuations or native I/O queues. Guest Abseil
  Status/time migration remains open. The candidate's copied-project and
  two-backend GUI tests passed (3 tests), root CTest passed 8/8, and the
  candidate was promoted to `~/dev/symbian-sdk`. Its 2,596-file predecessor
  remains at `~/dev/symbian-sdk-before-timer-cap-20261002`; both verify by
  digest. Post-promotion ARMv6/Dynarmic normal and changed-result timer
  controls passed (2 tests), as did the canonical copied-project test.

* Generated applications now have an opt-in `SYMBIAN_ENABLE_TIMER_TASKS`
  profile. It links `Symbian::Stackless` and uses one event-thread wait for
  Window Server input/redraw and A11-derived timer Tasks. A tap logs
  immediately and schedules a delayed log; Clear cancels pending Tasks.
  Actual RM-807 GUI controls passed on Dynarmic and Dyncom, including delayed
  completion, cancellation and normal Exit. The default GUI controls passed
  on both backends, and a default E71 starter built and executed. The selected
  DRTAEABI proxy now includes the real `__cxa_pure_virtual` ordinal needed
  by this link. The 2,596-file candidate was installed at
  `~/dev/symbian-sdk`; the previous sealed version is retained at
  `~/dev/symbian-sdk-before-window-timer-20261002`. Both 2,596-file seals
  verify; canonical copied-project and two-backend opt-in GUI tests passed
  (3 tests), as did root CTest 8/8. This establishes bounded
  shared-loop integration, not complete C1/C2, guest Abseil or fibers.
  `std::chrono` is provisional in the public timer API: once guest Abseil
  time is executable, SDK public time utilities should use Abseil types.

Earlier 2026-10-01 checkpoints:

* The first native-to-A11 stackless completion path now executes through
  `symbian::concurrency::TimerPump`. One event OS thread owns the native
  `RTimer` requests and publishes `Task` results; a worker can request
  cancellation through a coalesced `RThread::RequestSignal()` wakeup without
  touching thread-relative timer handles. A bounded guest control verifies
  parked cross-thread cancellation, monotonic `ScheduleAt`, overlapping
  timers, continuation reentry, immediate `OnReady`, close with a pending
  timer and heap-cell balance. Normal/changed controls passed eight source
  cases before a final continuation check, and the formatted source smoke
  plus eight installed SDK cases passed after it on ARMv5T/ARMv6 ×
  Dyncom/Dynarmic. The final candidate and canonical copied-project tests
  passed, as did a canonical ARMv6/Dynarmic smoke, root CTest 8/8 and both
  ARM IDE index builds. The 2,596-file canonical SDK and its 2,595-file
  backup verify by digest. This is not yet a shared Window Server event pump,
  arbitrary native I/O adapter, full A11 scheduler, or fiber backend.

* A bounded native `RTimer` owner now lives behind a narrow original-SDK
  bridge and a modern `symbian::concurrency::NativeTimer` interface. It creates
  a thread-relative handle, rejects negative and overlapping arms before the
  OS can panic, cancels and drains a pending request on close, and retains its
  status storage until completion. A probe exercises three simultaneous
  timers, completion before waiting, cancellation, close during a pending
  request, rearm and heap-cell balance. Normal and deliberately changed-result
  cases passed eight source-built and eight installed-SDK ARMv5T/ARMv6 ×
  Dyncom/Dynarmic executions. The formatted final export passed another
  eight installed cases; its copied/relocated project test and canonical
  ARMv6/Dynarmic smoke test passed. Root CTest passed 8/8, both ARM IDE probe
  targets built, and the 2,595-file canonical SDK and its 2,594-file backup
  verify by digest. That checkpoint established the C1 request-owner slice;
  shared Window Server pumping and broader C1 controls remain open.

* The guest monotonic clock now has concurrent execution controls: two real
  `std::thread` readers take 2,048 samples each, then exchange 128 ordered
  timestamps, with no net Symbian heap-cell increase. Normal and changed
  controls for this clock and the existing thread path passed 16 source and
  16 installed-SDK ARMv5T/ARMv6 × Dyncom/Dynarmic cases. The combined link
  exposed an obsolete `__throw_system_error` fallback, now retired in favor
  of original libc++ `system_error.cpp`; `std::this_thread::yield()` required
  the real `sched_yield` libc import. The previous error-category probe passed
  eight source cases. An invalid `std::thread::join()` exited with -6 in four
  source and four installed negative controls, confirming the default fatal
  no-exceptions path. The new SDK candidate has 2,594 digest-valid files;
  an E71 generated starter built and executed, copied/relocated project tests
  passed before and after promotion, a canonical ARMv6/Dynarmic clock-thread
  smoke test passed, root CTest passed 8/8, and both ARM IDE probe builds
  include the new source. It was installed at `~/dev/symbian-sdk`; the prior
  sealed SDK remains at `~/dev/symbian-sdk-before-clock-thread-20261001`.
  A shared timer/Window Server pump, suspension and half-wrap clock
  continuity remain open C1 gates.

* The guest now executes original libc++ `steady_clock` and `system_clock`.
  RM-807's OpenC `CLOCK_MONOTONIC` returned `EINVAL`; a maintained Symbian-only
  LLVM patch reads `NTickCount` and its HAL period for monotonic time, with an
  ordinary-tick fallback. Original Symbian source recommends `FastCounter`
  for short profiling and `NTickCount` for production. Two ordered EKA2L1
  patches expose the nanokernel/fast-counter HAL rates and correct the
  emulator's FastCounter to its advertised 32,768 Hz. Positive and changed
  guest controls passed 16 ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source
  and 16 against the sealed installed-SDK candidate. The generated E71
  starter also built and executed with the new optional proxy catalog, and
  the copied/relocated SDK project test passed both before and after
  promotion. The converter now accepts an
  actual needed-proxy subset while retaining identity and duplicate checks;
  its import suite passed 11 tests with one opt-in skip. Root CTest passed
  8/8; both ARM IDE probe targets build and index the two new clock sources.
  The 2,594-file sealed SDK was installed at `~/dev/symbian-sdk` with its prior
  2,591-file tree at `~/dev/symbian-sdk-before-clock-20261001`; both digest
  sets verify. Concurrent clock reads, suspension and a gap of half a 32-bit
  tick wrap (about 24.9 days at 1 ms) remain C1 gates.

* The prior root IDE checkpoint exposed all 48 then-known sources across
  platform-targeted probe projects through per-project ARM CMake targets,
  including the C++20 module and the locally prepared Mbed TLS probe.
  Separate ARMv6 and ARMv5T
  guest-index presets built successfully; both `compile_commands.json` files
  cover 48/48 sources with the correct target triple. The Mbed TLS source and
  SDK header paths are local opt-in settings, not shared repository paths.
  The local ARMv6 CLion preset uses LLVM 23 and
  was enabled alongside the existing host Debug profile, preserving a backup
  of the prior workspace settings. The running IDE log records a successful
  `clion-guest-probes-armv6` CMake generation (exit 0) after the profile was
  enabled. Actual editor diagnostics/header navigation remain a UI check;
  terminal compile database evidence is separate.
* A bounded classic-C locale and stream runtime is now a maintained,
  installable `Symbian::Streams` profile. Original LLVM libc++ locale/ios/
  ostream/iostream/strstream sources, SDK C-locale adapters and real selected
  OpenC imports produce `std::ostringstream` output in a guest. C and POSIX
  locale names work; invalid adapter inputs return `EINVAL`. Normal and
  changed-result controls passed eight ARMv5T/ARMv6 × Dyncom/Dynarmic cases
  from source and another eight from a fresh installed SDK. The visible
  SDK has 2,535 digest-valid files; its verified predecessor remains at
  `~/dev/symbian-sdk-before-streams-20261001`. This alternate archive uses
  its own libc++ configuration and replaces `Symbian::Runtime` in a target.
  File streams, wide strings, arbitrary locales and executable guest Abseil
  Status/StatusOr remain open. The pinned Abseil build is still an isolated
  diagnostic, not a shipped guest library. An isolated pinned
  `absl_statusor` archive now cross-compiles with experimental Symbian/Fuchsia
  platform guards and wide declarations, but guest linking exposes absent
  low-level mapped allocation, 64-bit atomics, TLS, soft-float builtins and
  further C/POSIX services. No Status execution or full A11 C1–C3 claim follows.
  A narrow native `RChunk` bridge then passed 16 source and installed-SDK
  normal/changed-result executions across both ARM targets and CPU backends:
  query native page size, create, write, grow, check retained bytes and close
  16 local chunks. This is a candidate page-source prerequisite, not an
  Abseil allocator or a native
  request/fiber backend.
* Imported **function** pointers now cross the ELF/E32 boundary in a bounded
  form. The converter validates ARM PLT identity, the dynamic `R_ARM_ABS32`
  record, zero-initialized writable storage and its retained relocation,
  then writes an E32-relocatable PLT pointer. A global EUSER `memmove`
  pointer executed on ARMv5T/ARMv6 × Dyncom/Dynarmic from source and against
  the existing installed SDK runtime/proxy (eight executions). Two mutated
  ELF controls reject a data-object relocation and false PLT symbol value.
  Imported **data objects** remain unsupported; typed guest exceptions still
  need that distinct ABI support. The earlier isolated LLVM libc++ classic
  locale/stream prototype ran on both backends before its promotion above.
  Its first image exposed an unrelocated LLD
  interworking thunk and failed in the guest. The converter now relocates
  LLD's exact named eight-byte ARM long-branch thunk; the original stream
  image then exited 0 on both CPU backends. A maintained thunk call passed
  all four architecture/backend cases, while a mutated out-of-range target
  was rejected. A fresh visible SDK passed the combined 11-case
  function-pointer/thunk matrix, was promoted after digest audits, and now
  has 2,503 sealed files. The previous 2,503-file SDK is retained at
  `~/dev/symbian-sdk-before-import-pointers-20261001` and also verifies. A
  post-promotion ARMv6/Dyncom imported-pointer execution also passed through
  the canonical SDK manifest.
  Other linker-generated absolute thunk forms remain open.
  An isolated pinned-Abseil `absl_status` build with the experimental stream
  profile progressed further but still fails on Linux `link.h` assumptions,
  disabled wide strings and unavailable file streams. No guest Abseil archive
  or A11 native request/fiber backend is claimed; the compiler log is retained
  under `.symbian/abseil-guest-probe/`.
* Clang-compatible guest varargs and original LLVM libc++ error categories
  now execute on the named firmware fixture. The SDK-owned `stdarg_e.h`
  bridges OpenC's header to Clang's ARM EABI `va_list`; a real `libc.dll`
  `vsnprintf` formats register, stacked and 64-bit arguments. The guest
  archive builds original `error_category.cpp` and `system_error.cpp`, and
  `std::error_code` category/message/condition checks use actual libc imports.
  Their first PIC image was correctly rejected for cross-mapping code/data
  references; compiling those originals without PIC uses the existing E32
  data relocation. Normal and changed-result controls passed 16 source-tree
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases, and another 16 against a fresh
  installed SDK. A copied-SDK application build passed. The visible SDK was
  refreshed with 2,503 digest-valid files; the previous 2,502-file tree is
  retained at `~/dev/symbian-sdk-before-runtime-c-services-20261001`.
  The promoted canonical SDK passed four ARMv6/Dyncom normal and
  changed-result controls for both capabilities. Root CTest passed 8/8;
  Black, Ruff, clang-format and `git diff --check` passed.
  An isolated pinned-Abseil `raw_logging_internal` target compiles with the
  maintained varargs header; the full Status target still fails on streams.
  Abseil Status still needs the unbuilt locale/iostream closure; A11 native
  request ownership, `Await` and fibers remain open.
* The bounded guest completion profile now owns its API under
  `<symbian/concurrency/*.h>` and `symbian::concurrency`, including its
  OS-thread mutex adapter. It no longer exports incompatible headers under
  A11's `<a11/concurrency/*.h>` or `thread::` names. The unmodified A11 source
  pin still has its original namespace and remains the reference for a full
  guest port. Source and fresh installed-SDK execution each passed eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed-result controls; a copied-SDK
  generated application build passed. The visible SDK was refreshed from the
  prepared workspace and both its 2,502-file manifest and the retained
  `~/dev/symbian-sdk-before-namespace-20261001` tree verify by digest.
  The canonical SDK also passed the ARMv6/Dyncom normal and changed-result
  controls after promotion. Root CTest passed 8/8, and the A11 source
  integrity group passed 5/5.
  Guest Abseil Status/StatusOr, A11 Await and fibers remain open gates.
* A bounded A11-derived stackless guest profile now executes
  Promise/Future/Task, inline `OnReady`/`Then`, cooperative cancellation,
  abandoned-promise completion, ordered nonblocking `JoinAll` and bounded
  reentrant `DriveInline` and `TaskGroup::Finish` fan-in/cancellation. Its
  `symbian::concurrency::Mutex` adapter uses the tested guest libc++ OS-thread lock; no fiber
  enters it. The normal and changed-result execution matrix passed eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source and another eight against
  a fresh exported SDK through `Symbian::Stackless`. The visible SDK was
  refreshed after a clean 2,496-file audit; its new 2,502 files verify by
  digest, and the previous tree is retained at
  `~/dev/symbian-sdk-before-stackless-20261001`. This is an explicit
  `symbian::concurrency::Result` adaptation, not
  the final Abseil Status ABI or a native-request/fiber backend. Directly
  cross-building pinned Abseil Status revealed missing streams, C++ ABI,
  signal APIs and lock-free atomic assumptions. A11 C1, remaining C2 and C3
  work remains open. Root CTest passed 8/8 and the refreshed-SDK
  generated-project/A11 source group passed 15/15. The debugger check now
  stops in both the C bridge and `model.cc`. After the final TaskGroup
  cancellation routing adjustment, two ARMv6/Dyncom source controls and two
  canonical-SDK controls passed; the whole architecture/backend matrix was
  last run immediately before that narrow adjustment.
* Exception-capable guest work advanced without changing the default profile.
  An isolated ARM probe retains `.ARM.extab`/`.ARM.exidx`, an original-layout
  four-word Symbian exception descriptor, and E32 header offset. The converter
  now validates those bounds and supports a real ARM branch to the pinned
  Symbian EHABI personality export. A no-throw landing-pad/cleanup probe and
  changed-result control execute on ARMv5T/ARMv6 with Dyncom/Dynarmic (eight
  passed); two installed-archive cases also pass. A typed throw still requires
  `_ZTIi` imported object data through `R_ARM_GLOB_DAT`, which the resolver
  explicitly rejects; a maintained negative test confirms that gate. There is
  no advertised exception-enabled application profile yet. All eight macOS
  root CTest targets pass after updating the starter bridge test source list.
  The final focused Pytest run passed nine cases (eight execution controls and
  the typed-throw negative gate). The refreshed 2,496-file visible SDK and its
  retained predecessor both verify by digest; the previous tree is at
  `~/dev/symbian-sdk-before-exception-metadata-20261001`. The exported SDK's
  own Python/native module converted and inspected a descriptor-bearing ELF.
  A final E32 regression rejects a renamed ARM exception-index section without
  a descriptor. The rebuilt visible SDK includes that check; its immediately
  preceding verified tree is retained at
  `~/dev/symbian-sdk-before-exidx-validation-20261001`.
* Original LLVM libc++ `memory.cpp`, `thread.cpp`, mutex, condition-variable
  and future-state sources now build with a threaded guest configuration.
  `unique_ptr`/`shared_ptr`/`weak_ptr` lifetime and a `std::thread` worker
  execute with changed-result controls on both ARM targets and both emulator
  backends (16 passed). The thread case moves a unique owner into the worker,
  copies/releases shared ownership, joins, and checks 4,000 atomic increments
  and balanced allocation cells. The same 16 cases pass against a fresh
  2,496-file digest-valid SDK export using its prebuilt archive and standard
  EUSER/pthread/C++ ABI proxies. The generated starter now puts the C ABI
  boundary in `app_bridge.cc`; `model.h`/`model.cc` expose typed application
  code without opaque pointers or C linkage. Its copied/relocated build and
  four live GUI/allocation-failure cases pass. Standard futures still need
  exception-pointer/error-category support. Guest exceptions are a required
  opt-in profile, with the SDK default off; ARM unwind tables, Symbian's
  exception descriptor and throw/catch controls are current gates. Host native
  libraries retain their no-exceptions Status policy. Actual A11
  Future/Task/fibers remain open.
  The final 2,496-file export and visible `~/dev/symbian-sdk` both pass their
  digest audits; the previous 2,428-file tree is retained at
  `~/dev/symbian-sdk-before-threads-20261001`. Installed `Symbian::Threads`
  execution passed two ARMv6/Dyncom controls, generated wizard/copy/build
  tests passed, and all eight macOS root CTest targets passed.
* A bounded secondary-thread prerequisite now executes on the preserved
  RM-807 fixture. Primary and worker `RThread`s each perform 2,000 shared
  32-bit atomic increments; normal and changed-result controls pass all eight
  ARMv5T/ARMv6 × Dyncom/Dynarmic cases. The worker uses its own Symbian heap,
  and the parent drains `Logon`, checks exit reason/type, then closes the handle.
  Generated starter startup now accepts the OS secondary-thread entry while
  keeping process-global initialization on the primary thread. The fourteenth
  ordered EKA2L1 patch maps an observed Belle `RThread::ExitReason` SVC 0x34;
  its applied/replay checks pass. The same eight cases pass against the freshly
  exported SDK's prebuilt runtime archive and standard EUSER proxy; a generated
  project builds after SDK copy and project relocation. The visible 2,428-file
  SDK and its retained predecessor are digest-valid. This does not establish A11 tasks/fibers,
  general thread/TLS cleanup or hardware speed. Measured versus hypothetical
  costs are tracked in [PERFORMANCE_CONSIDERATIONS.md](../doc/docs/capabilities/performance.md).
* Bounded global C++ lifetime now executes: SDK EXE startup runs `.init_array`
  after heap setup and `__cxa_finalize`/`.fini_array` before exit; real
  `std::string` globals pass both ARM profiles and emulator backends. The SDK's
  default C++ DLL entry similarly runs a constructor on the actual Belle
  process-attach call list. Eight maintained DLL execution cases pass, including
  a changed-constructor negative control. The process-attach path required an
  explicit relocatable ARM-to-Thumb entry target and an ordered EKA2L1 patch
  mapping the observed Belle 0x10D library-entry-start operation. DLL detach,
  TLS, local-static guards, hidden/internal data and broader C services remain
  open. The direct test launcher had omitted Qt's macOS foreground-transform
  flag; with both SDK launch flags, the foreground app stayed unchanged across
  the live DLL run. A shared helper now supplies both flags to launchers. The
  28-case guest runtime execution matrix passed; four original-validator cases
  passed after correcting an obsolete exact-BSS-size assertion for the added
  destructor registry. The 12 ordered emulator patches replay from the pinned
  revision. Fourteen generated-project/library tests and all eight root CTest
  targets pass. The visible `~/dev/symbian-sdk` has 2,427 verified payloads;
  its prior sealed tree is preserved at
  `~/dev/symbian-sdk-before-global-lifetime-20261001`.
* The dynamic-library gate advanced: native E32 conversion accepts bounded
  DLL data/BSS and typed relocations; an independent EKA2L1 loader executes a
  frozen-export DLL with initialized data, zeroed BSS and GOT fixups twice in
  fresh processes on Dyncom and Dynarmic. Four oracle cases and two reproducible
  Python build cases pass. The eleventh replayable EKA2L1 patch fixes a Dyncom
  post-exit fetch after the killed process is unmapped. The installed SDK now
  supplies an ARM C compiler, `symbian_add_dynamic_library`, exact frozen DEF
  conversion, optional import proxies and retained ELF symbols. ARMv5T/ARMv6 C
  DLL and consumer-import CMake tests pass. The user's Mbed TLS adaptation's
  full 80-object `mbedcrypto` C archive builds with the SDK compiler, and a
  SHA-256 subset links/converts to a DLL with selected EUSER imports. The full
  library has not executed in a guest; internal/hidden writable globals,
  DLL detach/TLS/lifetime and module debugging remain open. Direct GUI test
  launchers use the shared nonactivating environment and nonbundle symlink.
  The 19-test focused
  DLL/library group, optional actual Mbed TLS integration, eight root CTest
  targets, Black/Ruff and patch replay checks pass. The visible
  `~/dev/symbian-sdk` was deliberately rebuilt and all 2,422 sealed payload
  files verify; its previous unmodified 2,420-file tree is retained at
  `~/dev/symbian-sdk-before-dll-data-20261001`.

* The latest full optional-input run passed **270 tests**, with one inherited
  Starlette warning (`.symbian/arm-full-background-verified.log`, 428.58 s).
  The visible `~/dev/symbian-sdk` was deliberately refreshed to the tested
  ARMv5T/ARMv6, ROM/Z and static-archive payload (2,420 verified files); its
  previous unmodified 2,410-file tree is retained at
  `~/dev/symbian-sdk-before-arm-profiles-20261001`. Installed commands resolve
  E6, E71, 7610, C7 and 808 firmware IDs. An installed-SDK `symbian init`
  generated and initially built an E6 ARMv6 app, recording RM-609 selection
  without changing owner applications. The optional GUI run and all eight root
  CTest targets pass; full-screen Space placement remains unverified.
* SDK-owned macOS emulator sessions now start from a private symlink outside
  the `.app` bundle and use the tenth replayable EKA2L1 patch to show/order Qt
  and OpenGL windows without requesting focus. A real GUI launch rendered a
  720×1280 capture while iTerm2 remained frontmost; the directly owned child
  was reaped. The OpenGL window is assigned normal managed desktop-Space
  behavior, excluding full-screen auxiliary display and tiling. The same
  launch path is used by direct GUI/runtime/debug tests. After rebuilding the
  patch, a Dynarmic GUI test passed and Chrome PID 60698 stayed frontmost
  before, during and after it. Actual placement while another app is
  full-screen and Linux focus remain validation gates.
* ARMv6 is the default for new builds and `symbian init`; developers can select
  ARMv5T in the wizard or with `--architecture`. Generated projects keep one
  architecture setting, use its matching guest runtime archive and report an
  unsupported ELF/ABI before publishing or launching. Existing ARMv5T projects
  retain their selection. A real ARMv6 `REV` and the ARMv5T software sequence
  execute on Dynarmic and Dyncom; the full runtime matrix passes 28 cases
  including independent original E32 validation. The root CMake file API now
  supplies an explicit ARM target to CLion's guest compiler probe while host
  tools remain native arm64; both compiler probes and all eight CTest targets
  pass. The architecture/A11 source checks pass 14 tests.
* Bounded writable EXE data/BSS now runs on Dynarmic and Dyncom: initialized
  values, 64 zero-filled BSS words, data/BSS mutations, code/data pointers and
  a Thumb function pointer. A changed initial value exits -115. Twelve runtime
  execution cases plus two independent original checksum/whole-image-validator
  checks pass (14 tests, 57.97 seconds). Four new native data/fixup tests and all
  eight root CTest targets pass. New-project linker layouts include the verified
  RW mapping. Bounded global constructors/destructors now execute; TLS and full
  DLL lifetime remain unsupported. Artifacts: `.symbian/runtime-data-oracles*`.
* Actual A11 thread/concurrency adoption has begun: 46 original licensed files,
  92 local include edges, per-file digests, original tests, no adaptations yet.
  Original Git comparison, CMake source checking and five tamper/closure tests
  pass. This is source staging, **not a built guest concurrency backend**;
  Boost forced unwind/exception boundaries require the planned explicit
  adaptation. Guest Abseil, shared ownership/atomics, static lifetime and OS
  thread/TLS prerequisites remain gates. No scheduler lookalike or premature
  tasks/futures/fibers examples were added.

* Shared ROM/Z onboarding is implemented: native original EKA2L1 archive,
  ROM/RPKG, extracted Z and VPL readers; portable content IDs/bundles; XDG data
  storage and retained cache evidence; global -> SDK -> project -> command
  precedence with explicit origins/mappings. SDK/build/init require no firmware.
  Run/Debug no longer hard-code RM-807 paths, and select its workaround only for
  the exact known pair. Six other dump imports are tested; C7, E6, 6120 and E71
  generated starters pass initial build, greeting, clock input and native normal
  exit using the default profile. 7610/P900 import, with a specific missing EKA1
  startup/import ABI error before application launch. Default fonts, compact
  layout and logical capture dimensions remove 808-only display assumptions.
  Unknown FBS requests return KErrNotSupported instead of hanging; opcode 0x2C
  itself remains unimplemented. Both new upstream patches replay/reverse exactly.
  Firmware checkpoint full optional-input run: **234 passed**, one inherited Starlette warning,
  no skips, 335.11 seconds. Eight root CTest targets and five native firmware
  GTests pass. Logs/artifacts: `.symbian/firmware-full-verified*`; earlier failed
  captures, the 6120 teardown hang and import diagnostics remain retained.
  See [FIRMWARE.md](../doc/docs/guides/firmware.md) for commands, portability, limits and migration.

* Bounded local GOT conversion now emits E32 text fixups for up to 1,024
  defined object/function slots, preserving Thumb state and linked GOT_PREL
  words. Eight runtime cases pass on the preserved Delight fixture: ordinary
  success/OOM, global std::nothrow plus constant/function GOT success, and a
  changed-constant failure on each backend. Six new native GOT tests and seven
  original-validator checks pass. Generated apps now acquire their model with
  std::nothrow and return -4 after closing native resources on failure; all
  fifteen project-generation/build checks pass, including both CPU backends
  and terminal GDB. The visible SDK was deliberately refreshed with its previous
  tree retained; owner application sources/settings were not rewritten.
  GOT checkpoint full optional-input run: 208 passed, one inherited Starlette warning,
  no skips (181.74 seconds); eight root CTest targets pass. Black/Ruff, changed
  C++ formatting and whitespace checks pass. Integrity checks confirm the
  original archive, all 13,438 golden files and all 2,408 current SDK digests.
* `symbian sdk install` exports visible target headers, matching libc++ runtime,
  ordinal proxies, CMake package, Python utilities/native modules, notices and
  command shims. Projects keep one local SDK setting and relative shared files;
  commands dispatch to the selected SDK, including its project templates.
* `symbian init` asks for identity and IDE preferences, builds initially, and
  generates actual CMake workspace/C++ module registration, an enabled stable IDE profile,
  Run executable and remote-debug profile selection. The initially missing
  workspace registration caused IntelliJ to open a generic module despite valid
  terminal CMake/clangd results. After repair, the actual reopened owner project
  `symbian-app-3` configures in the IDE and reports three sources, zero unknown.
  Earlier owner projects were repaired without replacing application sources.
  Generated Run/Stop and GDB are execution-tested; IDE toolbar/debugger interaction
  and full unwind coverage remain distinct gates. See PROJECTS.md.
* The hello/time starter renders original W32 font text, uses real home-time
  services, std::string/std::vector, Clear/Exit, focus and pending-request cleanup.
  Both CPU backends pass rendered tap/log/clear/normal-exit checks. Real GDB stops
  in the modern model and exposes clock arguments/source. Project/SDK copies,
  relative SDK selection and selected-SDK template dispatch pass integration tests.
* The preceding full verification passed 202 Pytest cases with all optional inputs and
  one inherited Starlette deprecation warning. Following the IDE registration
  amendment, all thirteen project-generation/build cases pass; eight root CTest
  targets pass. The installed macOS wheel audit includes the new templates.
  Updated Linux aarch64 checks pass seven host CTest targets, installed wheel
  closure/resource audit and 62 installed Python cases; Linux emulator remains
  unverified. Earlier test totals below describe earlier checkpoints.

* Root CMake exposes the real ARM `gui_app`, its `gui_app_e32` publisher and
  a native `gui_app_run` executable. The saved root GUI Run configuration now
  explicitly selects that executable. Terminal launch/Stop/normal SDK exit and
  GDB batch/MI relocation pass; actual reloaded IDE UI interaction is unverified.
* A11's actual native/Python Status/StatusOr bridge is ported, with canonical
  caster modules, payloads, Pydantic hooks, GIL helpers, provenance and licenses.
  Native codecs/tests remain exception-free. Python initializers expose shortcuts
  and serializable metadata uses Pydantic. See A11_STATUS.md for adaptations and
  inherited mapping/compact-MessagePack limits.
* A real bounded libc++ runtime executes heap-backed strings/vectors and cleanup
  on both emulator CPU backends; actual heap exhaustion/nothrow/fatal failure
  controls pass. Over-aligned allocation and original ARM compiler-rt division/
  remainder execute in the maintained probe. LLVM sources are pinned with one
  replayable Symbian atomic-query patch. See RUNTIME.md and
  ../CXX_CAVEATS.md. Bounded GOT, writable data and initialization now execute;
  general TLS and full hosted C++20 remain unsupported. Guest Abseil/JSON is
  not provided by the host wheel.
* Linux aarch64 CPython 3.12 host native tests, wheel, installed closure/resource
  audit and 62 Pytest cases pass. macOS's installed wheel audit passes too.
  x86_64/other CPython and Linux emulator/guest/debugger are configured/planned,
  not locally verified. DISTRIBUTION.md describes the one-install payload design;
  compiler/emulator/header/debugger payload wheels are not yet distributed.
* Earlier baseline verification passed 152 Pytest cases (40 optional skips) and seven
  root CTest targets. Runtime tests pass four firmware-backed cases plus a
  rejected GOT control; original validator acceptance/failure checks pass three
  GTests. Black/Ruff, generated stubs and root target-aware clangd checks pass.
  The copied HTTP tests emit a visible Starlette/httpx deprecation warning.

Earlier verification totals below are historical checkpoints; current evidence
and precise scope are recorded in RESEARCH_LOG.md.

| PLAN milestone | Status | Required evidence |
| --- | --- | --- |
| 0 Preservation | Archive tools tested; physical baseline pending | Device inventory, original artifacts, offline archive, tested human recovery appliance |
| 1 Toolchain | Reproducible ELF→E32; historical validation and ROMless process/DLL import tests pass | Matched Belle runtime, SDK imports and complete target ABI tests |
| 2 Project model | CMake/Ninja, persistent database, native SIS and ROMless install/launch pass | Matched SDK/DLL imports, general application support and Belle installer |
| 3 Emulator | Native arm64 build, ROM/Z import and real GUI pixels/input/zero exit pass on both backends | Lifecycle/reset/snapshot facade, full OS boot and general runtime coverage |
| 4 Automated development | Native build/package/check loops and opt-in rendered GUI tests pass | General unattended install/run/artifact loop and symbian test |
| 5 Modern debugging | Live ARM GDB stops/stepping, model inspection and native process-exit records pass | Full unwinding, panic/thread/module inspection and CLion debugger validation |
| 6–9 | Pending | Physical deployment/system/hardware/alternative OS gates in PLAN.md |

No physical-device executor, flashing capability or recovery automation exists.
Source inspection is not an emulator runtime test. Record implementation and
verification results in RESEARCH_LOG.md before changing milestone status.

Current working commands: `doctor`, `toolchain probe`, `toolchain verify-probe`, `toolchain verify-pointers`,
`toolchain verify-package`, `toolchain import-proxy`, experimental
`build`/`package`, ELF/E32/SIS/import-proxy `inspect`, `preserve create/verify`,
`emu status`, `emu screenshot`, `emu pointer`, and informational `device policy`.
The emu commands require an explicitly started private research endpoint; they
are not a general lifecycle API or a physical transport.
The e32_probe example has no SDK/imports/data/constructors; its direct thread
exit is a no-resource experiment. Both the linked ELF and converted E32 repeat
byte-for-byte in two builds on this host. Parser acceptance does not prove
Symbian loader compatibility. Reports retain loader/runtime verification false.
Artifacts and reports live under .symbian/. clangd consumed the generated ARM
compilation database with zero errors. Native core code is compiled with
exceptions disabled; the pybind11 boundary retains canonical status codes.

Verification: 112 Pytest cases pass with the patched emulator, native oracles
and public kernel source supplied. The 41 platform GTest cases and 48 independent
oracle GTest cases pass on Apple Silicon. The preceding emulator checkpoint
passed 288 upstream EKA2L1 cases; its sources are unchanged here. Black/Ruff,
clang-format and generated stubs pass; clangd target checks have zero errors.
Without optional dependency paths, four emulator smoke cases and two native
verification integration cases and one public-header research case skip.
One additional development DLL runtime case and three variable-header DLL
validation cases need the native oracles. Two additional pointer/combined-layout
cases need them.
EKA2L1's parser omits header CRC verification;
Nokia's original checksum and whole-image validator independently check it.
The unchanged historical validator accepts this image using host type adapters.
Both EKA2L1 CPU backends execute its ARM startup and Thumb C++ at two load
addresses; a changed input produces the expected failure exit. These are
ROMless CPU tests: the exit SVC is observed by their callback. Eight additional
cases use EKA2L1's real process loader, flexible memory model, scheduler and
kernel SVC dispatch under its epoc10 profile. Both backends return zero normally
and 42 for changed input; absent executables fail to create a process. Both
backends also launch another process after an earlier failure exit.
The process/thread exit states and address-space release are checked. This is
an import-free emulator process without Belle ROM/Z or system services.
Reports distinguish eka2l1_process_verified from Belle loader/runtime flags,
which remain false. No physical runtime ran.
Build and patch replay instructions are in research/eka2l1/README.md.

The CMake path preserves the baseline ELF/E32 bytes. Multi-source builds, header
changes, paths with spaces and cached no-op builds are tested. Its compilation
database points to existing build objects; clangd reports zero errors. The wheel
contains the CMake modules and builds the same E32 from an isolated installation.
Project instructions and the v2 build report are described in docs/BUILDING.md.

The unsigned native SISX package is reproducible and passes independent Nokia
UID/controller/data CRC checks. Six disposable EKA2L1 cases install the unchanged
probe, launch through the kernel on both CPU backends, reload its registry,
uninstall and reinstall. The registry's file hash agrees with an independent
hashlib baseline. That installer does not enforce phone signing/capability policy.
The production native inspector verifies checksums, SHA-1, E32 and the restricted
canonical profile before the trusted experiment. Reports retain Belle runtime
and phone installation flags false. See PACKAGING.md for profile limits and replay.
OpenSSL 3 is statically linked for SHA-1; its license is packaged with the wheel.

The native SDK component reads frozen EABI definitions and generates selected
function ordinal proxies using Clang/LLD. The public User::Exit slot remains 641,
and Nokia's unchanged ordinal lookup method agrees in a separate optional test.
An original e32std.h call compiles and links, typed layout assertions pass, and
clangd reports zero errors. The public request status is eight bytes with flags.
Proxy and link ELF builds repeat; SDK/source/header dependencies are recorded.
An isolated wheel installation includes the target probe resource and builds
the same proxy and linked ELF.
These are link contracts, not a verified 808 SDK. See SDK.md for replay.

The E32 import profile places function GOT slots in its code region and resolves
versioned symbols against original proxy ordinals. A compiled development DLL at
ordinal 7 executes on both EKA2L1 CPU backends; the patched slot, function PC,
changed-input failure and repeated launch are checked in six cases. Historical
validation/checksums accept both files. Two-DLL links and malformed controls pass.
An isolated installed wheel reproduces the same imported ELF/E32 and metadata.
General writable-data DLL support, matched SDK services, startup/cleanup and
Belle runtime remain open. See IMPORTS.md.


Native DLL conversion now resolves frozen function exports from retained ELF
symbols, preserves ordinal gaps/ABSENT entries, and emits the count prefix,
full absence bitmap and code relocations for all export pointers. No fixed
function address is required. Independent validation covers ordinals 7, 641 and
65,535, including complete variable headers and relocation pages. Both emulator
backends also verify the mapped table: present pointers agree with lookup,
absent pointers relocate to the entry, and the count word remains unchanged.
Combined import/export layout passes Nokia's checksum and whole-image validator;
its executable startup is not runnable DLL initialization evidence.

The earlier research DLL omitted the count prefix and export-pointer relocations.
Its successful structural validation and export lookup proved less than the
public ELF loader contract. It remains research material; maintained runtime
checks now use examples/dll_probe and the native converter. The installed wheel
reproduces the native DLL and ELF and their metadata. Evidence is in
.symbian/native-dll/report.json, verification-report.json and wheel-result.json.
General pointer relocations, writable data/BSS/TLS, constructors, SDK startup,
matched target DLLs and Belle runtime remain open. No device operation ran.


Retained internal ABS32 words now generate E32 text relocations, permitting
named RELRO tables within the RX mapping. A maintained multi-source C++ probe
executes an ARM callback, a Thumb callback and a virtual method, and reads a
constant-data pointer with an addend. Both emulator backends verify all four
mapped words and instruction state at the three indirect targets, changed-input
failure, repeated launch and address-space release. Its native SIS installs,
launches, reloads the registry, uninstalls and reinstalls in separate cases.
The new verify-pointers command retains 14 image/dispatch checks or 21 with the
package. An isolated wheel reproduces ELF/E32/SIS and repeats all 21 checks.
Evidence is in .symbian/pointer-probe, .symbian/pointer-package and
.symbian/pointer-check. See POINTERS.md for replay and the trusted-link contract.

Internal pointer relocations also coexist with eager imports and frozen exports
in independently validated layout cases. External absolute pointers, GOT_PREL,
writable data/BSS/TLS and global lifetime support remain open. Simple virtual
dispatch does not prove the full target C++ ABI. Matched Belle and physical
execution are still unverified; no device operation ran.


C++20 now has maintained language and named-module examples. Concepts/requires,
structural class arguments, consteval/constinit, designated initialization,
constrained lambdas, equality and small layout contracts compile with explicit
controls. The language probe passes 21 independent loader/installer cases. A
separately configured libc++ bit/concepts/span experiment produces distinct
machine code and passes the same loop without linking a hosted runtime.
The module example uses upstream Clang/CMake scanning, retains its BMI, checks
importer invalidation and passes the 35-case image/package loop. An isolated
installed wheel reproduces all three ELF/E32/SIS variants and all 77 checks.

All 124 Pytest cases pass with explicit optional compiler/header/oracle inputs;
the native platform and SDK ordinal checks pass. Evidence is under
.symbian/cxx20-probe, cxx20-check, cxx20-library, cxx20-library-check,
cxx20-module and cxx20-module-check. See CXX20.md. Complete standard library,
coroutine/thread/atomic runtime, global lifetime, writable data/BSS/TLS, SDK
services and matched Belle remain open. No physical-device operation ran.


The requested examples/gui_app and root WALKTHROUGH.md are now present. The
counter application directly uses Window Server; it has seven-segment drawing,
touch increment/reset/exit and explicit request cancellation/object cleanup.
Its primary-thread adapter attempts SDK heap/process setup and User::Exit,
while secondary-thread, exception-entry and global lifetime support remain
absent. The build report now correctly treats startup/cleanup as unverified
project behavior instead of assuming every executable uses direct ThreadKill.

At that checkpoint, the source profile staged 92 original header aliases and
native frozen proxies
for nine EUSER and 28 WS32 functions from pinned ignored public trees. Preparation
checks file digests, preserves original files/licenses, rejects malformed
profiles and output redirection, and records SDK/runtime verification false.
Clang/LLD and independent CMake trees reproduce the ARM ELF and E32. Five model
GTests pass, including wide/tall layouts, arithmetic, input bounds and saturation.
Original checksum/validator sources accept the generated GUI in eight cases.
DWARF verifies and LLDB resolves functions and source lines; clangd has no
diagnostics with its limited check-mode refactoring selection.

All 139 Pytest cases passed with explicit optional inputs; all six root CTest
targets passed. The final drawing-layout adjustment also passed the five model
GTests and all 15 GUI Pytest cases. An isolated installed wheel runs the SDK
preparation/build/verification CLI, reproduces both artifacts and repeats all
eight independent image checks. Evidence lives in .symbian/gui-sdk,
.symbian/gui-app, .symbian/gui-validation and .symbian/gui-research.

Visible GUI execution, actual Belle EUSER/WS32 compatibility, heap/cleanup and
guest debugger attachment remain unverified because matched ROM/Z is absent.
The walkthrough labels these procedures as future experiments. The example
has no application registration or SIS package; the existing SIS writer's
import-free restriction remains. No emulator OS boot or physical-device action
is claimed or performed. Phone RM/product/firmware details remain unknown.


The GUI now has a native single-EXE unsigned SIS package, independently validated
in 17 image/checksum/installer cases. Actual installer/registry behavior verifies
exact payload bytes, UID/SID/version and an independent legacy digest, reload,
uninstall and reinstall in disposable ROMless filesystems. A control records
the unchanged upstream loader's missing-library defect: it creates a process
with all 37 imported words still unresolved. No guest instructions execute in
these installer cases. Reports explicitly keep GUI/SDK execution and matched
loader/runtime verification false. The native writer now accepts imported EXEs
while still rejecting DLL payloads, resources and broader package profiles.
All 142 Pytest cases and six root CTest targets pass; the installed wheel
reproduces ELF/E32/SIS and repeats the 17-case package check.

The supplied Delight v1.8 ZIP was checked and staged privately. Its VPL declares
RM-807, product 059M7Q4 and version 113.010.1508; seven required/present files
pass archive and declared CRC checks. One opt-in native GTest successfully uses
EKA2L1's VPL/FPSX/ROM/ROFS/FAT importer into a new isolated root. The result
identifies Nokia/808 PureView/RM-807/epoc100 and supplies a ROM and actual system
DLLs, with 13,438 imported files inventoried by SHA-256. This corrects the prior
missing-assets state. Custom archive authenticity, stock recovery baseline and
matching this physical phone are not established.

A copied instance maps the GUI at 0x70000000, EUSER at 0x804bcce8 and WS32 at
0x80a4c028. It logs unimplemented SVCs 0x51/0xF7 and a $HEAP lookup failure;
visual behavior, correct startup/cleanup and debugger attachment are not yet
verified. TERM did not finish the private emulator, so its confirmed process
was stopped with KILL after retaining logs. The original imported root remains
separate from runtime state. No physical phone operation ran. Evidence is in
.symbian/gui-package[-check], gui-research/delight-archive-check.json,
delight-import.json, delight-import-inventory.json and the private instance logs.

Live ARM GDB 17.2 attachment now works against a disposable RM-807 instance.
The actual startup source breakpoint receives reason=0 and info=0x40ffc0 at
PC=0x700009da. A local GPL guest-debug-step.patch fixes silent execution after
single stepping. A real frontend/GDB Pytest proves two successive Thumb stops,
a stable fresh register read after a delay, source display and ROM SVC stops.
The same test fails with a 30-second GDB timeout when that patch is removed;
restoring it passes. All 143 Pytest cases with explicit optional inputs and all
six root CTest targets pass. This is live debugging evidence, not GUI success.

Stable registers show heap initialization returns KErrNotFound (-1), before
GuiMain. The ROM requests kernel HAL page size using SVC 0x51 and chunk creation
using 0x6D; the pinned epoc10 table maps those operations to 0x4F and 0x6B and
dispatches 0x6D as object lookup. The real exit path reaches unimplemented
0xF7, while its handler is registered at 0xF6. These firmware executive ABI
discrepancies require a fuller independently checked Belle profile. No whole
table shift, SDK replacement, or loader workaround was introduced.

Visible GUI output/input, successful heap setup, normal SDK exit, stack unwinding
and OS boot remain unverified. The test-owned frontend still requires KILL
after TERM; reaching an exit wrapper does not establish guest cleanup. The
original ZIP and all 13,438 baseline file digests are rechecked unchanged.
Physical phone details remain unknown and no device operation ran. Replays and
current limitations are in WALKTHROUGH.md section 9; transcripts, negative
control, test logs and integrity evidence are in .symbian/gui-research/debugger*.


### 2026-09-30 — Guarded RM-807 ABI and initial drawing-function execution

The real ROM export probe and source-wrapper comparison now support a piecewise
experimental Symbian 101 executive map. It has 170 existing handlers, while the
original 172-handler epoc10 map remains intact. Profile selection is explicit
and exact-ROM-digest guarded; real frontend tests reject unknown profile names
and changed private ROM bytes. This is a research profile for the supplied
Delight image, not universal Belle support or authenticated stock firmware.

With that profile heap setup returns zero and GuiMain executes. A separate ARM
TPIDRURO register fixes the original Dynarmic coprocessor abort, with four real
instruction/context cases passing on both tested macOS backends. Window Server
connection returns zero. A subsequent E32USER-CBase/69 panic exposed missing
cleanup-stack setup; the example now creates the SDK CTrapCleanup before GuiMain
and deletes it on return. Its frozen EUSER import count is now ten (38 total).

Live GDB verifies the initial DrawGui entry, zero/running model, 360 by 640 layout
and return after its guest drawing calls. The default map still fails heap
startup, preserving a useful control. Full DLL initialization remains unproven:
0x10D is still unimplemented. Rendered pixels, pointer delivery, normal cleanup/
exit, full unwinding, OS boot and physical-phone compatibility remain unverified.
No screenshot or visible-GUI success is claimed from a drawing-function stop.

All 146 Pytest cases pass with explicit optional inputs, all six root CTest
targets pass, and the two new research CTest targets pass seven GTest cases.
The opt-in ROM export probe passes separately, rejects existing/nested output,
and executes no guest instructions. All six patches apply in documented order
against fresh pinned source files. An isolated installed wheel reproduces the
updated ELF/E32, packages it and passes all 17 historical/installer checks;
research tests/firmware remain excluded. Black/Ruff and C++ formatting pass.

The original ZIP and every path, size and SHA-256 of the 13,438-file unbooted
baseline are rechecked unchanged. Runtime work uses copied instances; the
baseline is not booted. Phone identity and independent offline preservation
remain unknown. No device operation ran. Replays and boundaries are recorded in
WALKTHROUGH.md and docs/BELLE_ABI.md; private evidence is under
.symbian/belle-abi-research. The platform mission remains active.


### 2026-10-01 — Rendered GUI, pointer input, normal exit and developer workflow

The native GPL research adapter reads the actual guest screen texture, routes
logical pointer events through Window Server and copies kernel process-exit
records. It runs on existing Qt/kernel loops, adds no scheduler/thread/Python
callback and compiles with exceptions disabled using Abseil Status/StatusOr.
The wheel contains only the synchronous Python policy client for this endpoint;
EKA2L1 and the native adapter remain separate research binaries.

Live tests on Dynarmic and Dyncom verify 720x1280 portrait PNGs (logical 360x640,
scale two), 0000 → 0001 → 0002, an outside tap leaving 0002, reset to 0000 and
exit type kill/0 with reason zero for UID 0xe0000811. The frontend also exits
zero. Four native GTests exercise disabled/invalid/private-path startup bounds;
live tests reject malformed/oversized commands, traversal, duplicate outputs and
invalid pointers. Two observed locking/lifecycle mistakes were corrected:
pointer delivery must not retain the kernel lock that Window Server acquires,
and callbacks must detach before the OS worker destroys the kernel. Final native
status survives socket closure; saved reports are explicitly selected.

The GUI preset now provides both SDK import proxies without CLI injection.
Standalone CMake configure/build and clangd parsing/indexing pass with zero
errors using the limited documented tweak selection. Canonical ELF/E32 rebuilds
retain their previous hashes. The running IntelliJ IDEA/CLion-plugin instance
now has persisted Symbian ARM toolchain settings and a local clion-arm preset.
The exact local preset configures/builds and clangd reports zero errors. The GUI
project now configures successfully in the actual IDE. Its generated CMake API
lists app.cc, startup.cc and startup.S; the initial model resolves two C++
sources and one assembly source with zero unknown sources. The live preset
selects existing Default with explicit ARM paths because the running IDE had
not loaded the saved application toolchain name. The generated native GDB
profile selects the ARM debugger independently and does not need that restart.
The debugger frontend is untested. Setup, E32 publication and ARM remote-debug steps are in docs/CLION.md.

The initial control checkpoint passed 161 Pytest cases with explicit optional
inputs; the launcher checkpoint now passes all 170. All six root CTest
targets pass, and the three control/routing/register research targets pass eleven
GTests. Seven patches replay against 15 fresh pinned source files. Black/Ruff,
clang-format and whitespace checks pass. An isolated installed wheel outside
the source tree reads the native final exit envelope and inspects the actual
GUI E32 through its native extension. Its module paths, wheel digest and reports
are in .symbian/clion-setup/wheel-result.json. The research native
adapter and tests are absent from the wheel; Pillow is a development dependency.
The original ZIP and all 13,438 baseline paths/sizes/SHA-256 values are rechecked
unchanged. No physical-device operation ran; identity and independent offline
preservation remain unknown.

PLAN.md now records the completed vertical slices, evidence boundaries and
ordered next gates: developer onboarding, disposable lifecycle/symbian test,
unresolved ABI/DLL lifetime, bounded runtime/C++ library support, diagnostics,
then broader application/system/device scope. Full initialization (including
0x10D), 0xFF interception, writable data/TLS/static lifetime, full unwinding,
OS boot and physical compatibility remain unverified. The broader mission
remains active. Replays are in WALKTHROUGH.md and docs/EMULATOR_CONTROL.md;
private evidence is under .symbian/belle-abi-research/control* and gui-control*.

A bounded foreground GUI launcher now publishes the current E32 before Run or
Debug, copies the named golden, owns/reaps its frontend and retains manifests,
logs and final native status. Generated local GUI Run/GUI Debug configurations
and a native Symbian GUI GDB profile are installed in the dedicated GUI project.
The supervisor starts a halted instance before GDB, then relocates symbols after
connection using the actual mapping. Source breakpoint/variable checks and the
IDE's GDB MI2 protocol pass, including Thumb-to-ARM instruction stepping. Normal
Run exit, Stop cleanup, occupied ports and launcher-path quoting are checked.
IDE toolbar interaction and full debugger frontend/stack unwinding remain
unverified. This is not yet the general lifecycle/symbian test API.

The final full launcher/control suite passes 170 tests in 112.82 seconds with
all optional inputs enabled. Four targeted installer/ownership tests pass after
saving GUI Run as the default selection. Research CTest again passes all three
targets/eleven GTests. The installed wheel is exercised from /tmp, including
GDB version discovery without startup and default configuration selection.
The ZIP and all 13,438 baseline files remain unchanged.

Earlier full runs exposed an Exit-up request after the app had already closed
and one frontend that did not finish shutdown within 15 seconds despite guest
ThreadKill reason zero. Exit tests now send only down; the native adapter drains
already queued replies for a bounded interval when its Qt loop stops. Added
teardown phase logs and an owned-process stack sample on a repeat timeout.
Twelve repeated GUI runs, five six-case debugger/GUI groups and the final full
suite then pass. No repeat stack is available; the isolated shutdown timeout's
cause remains open rather than being inferred from later passes. IDE Stop still
has tested bounded cleanup.

The reopened GUI project scans 535 files and resolves its three target sources.
Root-project exclusions for private runtime/upstream/build data are saved with
the staged SDK unexcluded; their live application is unverified. IDE toolbar
interaction remains unautomated. PLAN.md retains these developer-lifecycle gates
before broader ABI/runtime and device scope.

The current runtime and DLL follow-up adds actual LLVM compiler-rt ARM EABI
64-bit signed/unsigned quotient and remainder, its shared C division core and
the ARMv5T-safe leading-zero helper. The expanded guest matrix passes 32 cases
on ARMv5T/ARMv6 and Dyncom/Dynarmic, including a changed-wide-result failure
control. A separate run with the optional oracle build passed four original
checksum/whole-image validator cases on the new ARM images.

The named RM-807 fixture now passes a bounded dynamic DLL lifetime test. The
real ROM `RLibrary::Load` initially reached an unimplemented Belle SVC 0x10E
and returned its unchanged `this` pointer. The thirteenth ordered emulator
patch maps that observed slot to EKA2L1's existing v10 load-preparation hook;
the loader-server operation, two ordinal lookups and `RLibrary::Close` then run.
A DLL destructor writes to client-owned memory before `Close` returns. The
maintained constructor, dynamic-load and absent-DLL matrix passes 16 cases
across both ARM profiles and emulator backends; absent DLL returns -1 and the
client exits -121. All 13 emulator patches replay from the pinned base and the
last reverse-checks. This does not prove all DLL modes, TLS or thread lifetime.

A direct genuine-A11 prerequisite probe compiled `std::make_shared` but failed
to link without libc++ shared-ownership definitions. The current libc++ profile
also disables threads, so merely adding `memory.cpp` would not establish A11's
atomic cross-thread ownership. C0 still needs a real thread/atomic backend and
guest Abseil before Promise/Future can be exposed. The pinned 46 A11 sources
and 92 local include edges still verify unchanged. No guest A11 scheduler or
fiber capability is claimed.

The visible `~/dev/symbian-sdk` was deliberately refreshed from the prepared
checkout after its previous 2,427 sealed files all matched their digests. The
previous SDK is retained at `~/dev/symbian-sdk-before-wide-dll-20261001`;
the new canonical SDK also has 2,427 verified sealed files. Its ARMv6 archive
contains the new `__aeabi_ldivmod`, `__aeabi_uldivmod` and `__clzsi2` symbols.
A dynamic DLL run against the refreshed SDK passed; a fresh `symbian init`
build/copy/relocation test also passed. No owner application source or UID was
changed.

The external user-owned `~/dev/mbedtls-symbian` sources were built without
editing them. A selected SHA-256 subset links into the SDK E32 DLL and now
executes through a dynamic guest `RLibrary` client: SHA-256 of `abc` matches the
known 32-byte digest, while changing the input fails with -132. Static format
and four Dynarmic/Dyncom execution/control tests passed (five total). This is
one useful Mbed TLS function, not broad TLS/network integration.

The native-lock prerequisite now has a guest execution probe: an EUSER
`RFastLock` is created, its uncontended `Poll` succeeds, a second `Poll` while
held returns `KErrTimedOut`, and `Signal`/`Wait`/close complete. Four ARMv5T /
ARMv6 client × Dynarmic/Dyncom cases passed in 71.31 seconds. This runs the
RM-807 ROM implementation for both client architectures; it does not prove a
separate ARMv5 ROM implementation or cross-thread contention. A11's fiber-aware
`thread::Mutex` still needs a backend that parks fibers instead of blocking
their event OS thread. Fast ARM context switching remains unimplemented.

The final development SDK export adds the real ROM `RLibrary` and `RFastLock`
imports to its standard EUSER proxy. Its 2,427 sealed files passed digest
verification; the installed-proxy fast-lock matrix passed four cases and the
Mbed TLS SHA-256 DLL suite passed five. The export was promoted to
`~/dev/symbian-sdk` after checking the previous canonical SDK's digests; that
previous tree remains at `~/dev/symbian-sdk-before-fast-primitives-20261001`.
Both retained and current trees have 2,427 digest-valid files. A fresh
generated-project initial-build, SDK copy and project-relocation test against
the canonical SDK passed in 10.41 seconds.

A further C0 runtime gate now executes 32-bit `std::atomic` fetch-add, acquire
load, failed/successful CAS and exchange. Clang emitted `__atomic_*` libcalls
for ARMv5T and `__sync_*` libcalls for ARMv6; both are backed by real EUSER
ordered atomic imports through a narrow original-header bridge. The normal
and changed-value controls passed eight cases across both client targets and
both emulator CPU backends in 83.77 seconds. A fresh installed SDK with the
new standard proxy and `e32atomics.h` passed the same eight cases in 79.98
seconds; its 2,428 files are digest-valid. These are single-thread paths.
Cross-thread race behavior, 64-bit atomics, libc++ shared ownership and the
A11 task/fiber backend remain unverified.

The 2,428-file atomic-enabled export was promoted to `~/dev/symbian-sdk`
after the previous 2,427-file canonical tree passed its digest check. That
previous SDK is preserved at `~/dev/symbian-sdk-before-atomics-20261001`;
both current and backup trees pass their sealed-file checks. A fresh
generated-project initial-build, SDK copy and project-relocation test against
the new canonical SDK passed in 7.42 seconds. The source profile now stages
93 pinned header aliases, including original `e32atomics.h`.
The real-header GUI source-staging/link test passed with the updated count
in 10.75 seconds.
Finally, a stricter test copied the runtime probe into a temporary project,
linked the installed SDK's prebuilt `Symbian::Runtime` archive and standard
EUSER proxy, and used its installed `symbian/runtime.h`. All eight atomic
normal/changed-control guest executions passed in 85.85 seconds. The earlier
79.98-second selected-SDK run had used the installed proxy with a runtime
rebuilt from this checkout; the stricter run verifies the exported archive.
Four default string/vector runtime cases then passed on the same prebuilt
archive across both ARM profiles and emulator CPU backends in 49.80 seconds.

The 64-bit atomic C0 prerequisite now has two complete runtime archives. The
default `Symbian::Runtime` uses an original EUSER `RFastLock` to serialize
64-bit load/store/CAS/exchange/add across threads. The opt-in
`Symbian::NativeAtomics64` calls original EUSER 64-bit exports through a
matching proxy. The preserved RM-807 ROM executes LDREXD/STREXD for these
operations. Dyncom had assembled the STREXD register pair but written its
register index; the fifteenth ordered EKA2L1 patch fixes that argument. A
ROM-independent STREXD GTest passes on both backends. This was an emulator
fault, not evidence of bad ROM atomics.

Original Symbian source also has an ARM V5/V6 interrupt-masking implementation,
so native EUSER imports are not automatically lock-free on other firmware.
The opt-in native profile was initially verified on the named RM-807 fixture
and was later checked on RM-675/RM-609 as recorded below. Clang's
ARMv6 `is_lock_free` builtin falsely described the default lock-backed
archive; a maintained pinned LLVM header patch now directs `std::atomic` and
`std::atomic_ref` runtime queries to the selected archive. The 64-bit probe
checks actual parent/worker high-and-low-word updates, direct `__sync` calls,
store, CAS, `atomic_ref` and changed-result controls. Twelve source cases
passed before the query extension, its three-path smoke passed after, and all
16 selected installed-SDK cases passed across both ARM targets and emulator
backends in 80.40 seconds. Root CTest 8/8 and the independent STREXD CTest
pass. The final visible SDK has 2,564 digest-valid files; its prior 2,535-file
tree is preserved at `~/dev/symbian-sdk-before-native-atomic64-20261001` and
also verifies. A fresh generated-project initial-build, SDK copy and
relocation test passed against the promoted SDK. Wider ordering litmus tests,
other firmware, physical hardware, guest Abseil linkage, C1 request ownership
and A11 fibers remain open.

A subsequent non-808 check broadened the verified native-atomic profile to
C7-00/RM-675 and E6-00/RM-609 under EKA2L1. Their five 64-bit EUSER entry
points have byte-identical prefixes to the RM-807 ROM, including LDREXD/
STREXD add and CAS loops. Cross-thread `std::atomic<uint64_t>` then passed on
Dyncom and Dynarmic for both devices (four cases, 31.91 seconds). The initial
run isolated an unrelated emulator gap: their v10 ROM wrappers use SVC 0x32
for `RThread::ExitReason`; the sixteenth ordered patch maps it to the existing
handler. The older 6120c/RM-243 and E71/RM-346 ROMs expose only 2,228 EUSER
exports, below this profile's five EABI ordinals. The native profile must not
be selected for them. Physical phones, additional dumps and broad atomic
memory-order tests are still unverified.

The next runtime slice links original libc++ `hash.cpp` and genuine LLVM
compiler-rt soft-float, aligned-copy and multiply helpers. A bounded
`std::unordered_set<int>` growth/erase/cleanup probe and changed-result
control passed eight ARMv5T/ARMv6 × Dyncom/Dynarmic cases from source and
eight against a sealed installed-SDK candidate. The companion compiler-rt
normal/negative matrix also passed eight source and eight installed cases.
The candidate has 2,591 digest-valid files. Hash-table growth uses the ROM's
`ceilf` from `libm.dll`; this is not guest Abseil `flat_hash_map` yet.
An E71 generated starter initially failed because the CMake runtime target
added an unused `libm` needed proxy. Its CMake link now scopes LLD
`--as-needed` to that proxy. The E71 starter then built and executed on its
named firmware, and an installed hash-table smoke test still imports and
executes `ceilf`. ROMs without `libm.dll` still cannot execute that hash-table
path until the SDK supplies a math implementation.

The corrected candidate's full installed hash-table matrix passed all eight
cases in 44.76 seconds. A final re-export from the prepared checkout had
2,591 digest-valid files, and its initial-build/copy/relocation test passed.
It was promoted through the SDK installer's path rebasing to
`~/dev/symbian-sdk`; the previous 2,564-file SDK is retained at
`~/dev/symbian-sdk-before-hash-table-20261001`. Both trees verify all sealed
file digests. Root macOS CTest passed eight targets. The public PyPI payload
closure and broader guest Abseil/A11 execution remain open.

### 2026-10-02 PC Suite host USB transport

The connected Nokia 808 in owner-selected PC Suite mode was inspected with a
statically linked libusb 1.0.30 host extension. The default `device info` map
now includes endpoint directions/types and CDC unions. Opt-in AT battery and
signal codes, a read-only MTP session with two reported storage records and
bounded root listing, and a PC Suite OBEX Connect/Disconnect handshake were
observed. Both OBEX responses were `0xA0` after Disconnect included the
server-issued Connection ID. A low-level Python binding exposes synchronous
and queued asynchronous control/bulk/interrupt transfers with an explicit event
pump; the Connect/Disconnect sequence was repeated through that binding.
`otool -L` showed no dynamic libusb dependency; the wheel also packages the
static archive, header, and LGPL text for the host SDK. The installed wheel
audit passed, and the matching source distribution includes the new native
transport source and the pinned libusb build recipe. These observations do
not establish file transfer, SyncML, PC Suite command semantics, or debugger
access. See `docs/DEVICE.md` for the opt-in commands and `docs/RESEARCH_LOG.md`
for the experiment record. Focused device and CLI tests passed (37); the
full Python suite remains red in toolchain/image and verification cases
(165 passed, 356 skipped, 7 failed, 39 errors).

The host USB binding now returns typed native records and exposes A11-backed
transfer Futures through `AsyncUsbSession`. Python inspection and transfer
results are Pydantic models with documented fields and omitted unobserved
defaults. On the connected 808, typed descriptor, MTP, AT, and OBEX probes
retained their prior observed results; an awaited control read returned two
bytes, a pending interrupt Future cancelled, and the Future-driven OBEX
handshake returned `0xA0` to Connect and Disconnect. The focused device, CLI,
and model suite passed 44 tests. Bare `symbian` now prints top-level help
and exits successfully. Terminal help uses colour and clear section/option
emphasis; redirected help remains plain text. The wheel built, a clean virtualenv loaded
the typed native list API and Pydantic models, and its installed audit passed.
The matching source distribution contains the new native and Python sources.

Application-facing CLI help, Python docstrings, native binding descriptions,
SIS comments, and build guides now call application builds and packages
"applications". Existing `*-experiment` manifest kinds and report schemas
remain unchanged. Focused CLI and device tests passed (40). The project-build
and toolchain tests had 8 passes and 3 existing ARM unwind descriptor failures;
the wording changes did not touch that conversion path.
The rebuilt wheel passed its installed audit in a clean virtualenv, including
the updated native binding descriptions; the matching source archive built.

### 2026-10-02 Console interaction and device reconciliation

The desktop console now uses a shared sidebar and status bar for resolved
workspace, application, SDK and phone state. Application and firmware wizards
can skip optional pages, while additional settings remain available on demand.
Technical text uses a borderless native frame and host-derived font sizes.
Supported-phone views refresh on a five-second inventory cycle; a redacted
serial identity preserves selection across reconnects and mode changes, while
port-only identity is cleared after an observed disconnect. Stale phone probe
results are removed. `symbian console` launches a detached GUI process and
returns to the terminal. Focused console and device tests passed (27); a
hidden Tk smoke check loaded all five tabs and observed one selected phone.
The USB inspector, Protocols and Activity now scroll inside fixed side/status
chrome. Phone map and protocol results are cached by selector and applied
immediately when revisiting a phone; read-only USB mapping refreshes behind
the cached view. Profiling on this host measured CLI import at 0.147 s,
catalog at 0.001 s, USB inventory at 0.008 s, and the initial phone context
at 0.977 s. A USB-signature discovery cache reduced repeated unchanged
context calls to 0.004–0.005 s, with full revalidation after 20 seconds.
Focused console/device tests passed (28); a hidden Tk smoke check confirmed
the Protocols scrollbar appears for overflow at a 1000×620 window.
The expanded console, CLI and device suite passed 57 tests. The macOS ARM64
wheel rebuilt with the console modules, passed an installed-wheel audit in a
clean Python 3.12 environment, and that installation loaded 38 tasks and one
connected phone through the in-memory client.
An actual `uv run symbian console` invocation returned in 0.12 seconds with
exit code zero; the detached GUI child remained running. The wheel was rebuilt
and re-audited after the final scroll and device-label changes.
The generic Home/Tasks browser was removed. Six main tabs now group the 38
actions by application, firmware, emulator, SDK, device and activity purpose;
SDK and Devices have focused inner sections. A hidden Tk check verified every
action appears exactly once, all tab sections mount, and cached tab switches
completed in 0.0002–0.001 seconds on this host. The bridge now cancels its
scheduled drain callback during shutdown.
Guided action sections were also placed in scrollable viewports, so a long
review or result can be reached without moving the fixed sidebar/status bar.
After this navigation change, the 57 focused console/CLI/device tests still
passed, and the rebuilt wheel passed the installed audit again.
Technical result viewers now request 14 lines and ask their enclosing viewport
to reveal them when expanded. A hidden Tk layout check measured a 211-pixel
technical text area and a scrolled outer viewport with the scrollbar visible.
The empty action-form canvas observed on the no-input `doctor` action was
removed. No-input actions now present a direct run button; other forms fit
their actual content up to a 280-pixel cap and scroll only when needed. Hidden
Tk checks confirmed the doctor form container is absent, application inputs
use the capped scroll area, and direct doctor execution produced its outcome.
After this form change, the 57 focused tests still passed, and the rebuilt
wheel passed its installed audit in the clean Python 3.12 environment.
The GUI now retains the Aqua control theme with native selected-content blue,
a contrasting sidebar, white content surfaces and alternating table/action
rows. Hidden Tk checks confirmed the Aqua theme, selected colour `#0064e1`,
row tags, and touchpad wheel routing from an input to its form and then the
outer view at the form's edge.

### 2026-10-02 — desktop console frontend revision

- `symbian console` now selects a pywebview desktop shell, with bundled local assets and the existing FastAPI/httpx in-memory service. macOS uses WKWebView; Windows uses WebView2; Linux receives the PySide6 renderer dependency. wxPython is no longer a package dependency or launcher choice. The CLI still detaches from the terminal.
- Six icon-led sidebar sections and compact tabs replace nested subnavigation. Action cards use task icons and omit repeated section labels. Guided forms, cached view state, background context refresh, status indication, dedicated USB/protocol views and technical details remain available. The USB table uses named class labels, and inspected phone interfaces appear as a separate interpreted table.
- A live Cocoa smoke check loaded 38 actions and one connected 808 and navigated Devices/SDK tabs without JavaScript errors. The focused console tests passed (21), JavaScript syntax and Python lint/format checks passed, and the built wheel contained all local frontend assets. No device protocol request was made for this UI revision; Windows/Linux visual testing remains open.
- The follow-up presentation pass replaced nested sidebar entries with three-tab strips inside SDK tools and Devices, added consistent action and protocol icons, removed repeated section labels and an empty no-input form hint, and tightened action-card spacing. A live Cocoa check exercised tab navigation and a host-only doctor action. The final built wheel passed a clean installed-wheel audit; 21 focused Pytests, Ruff, Black and JavaScript syntax checks passed. `uv run symbian console` returned in 0.12 seconds and opened the WebKit-backed desktop process.

### 2026-10-02 — integrated console inspection

- Firmware now displays its imported catalog immediately, with search, selected-record details and source settings in a contextual right sidebar. Import, Inspect, Export and source probing open while the catalog stays visible. Emulator settings, host readiness and connected-phone inventory likewise load as live views alongside their related actions. All other action results have a bounded structured presentation; large firmware manifests expose a searchable file index before optional raw JSON.
- Required inputs are ordered before optional source settings, and simple actions run without a redundant review page. The optional-settings control no longer skips required steps. A live macOS check showed seven imported identities, an Export form prefilled with `e6`, an inspected firmware file search, effective emulator cards and the connected 808 card. No new phone protocol transaction or export was run. Twenty-two focused tests, Black, Ruff and JavaScript syntax passed; the rebuilt wheel passed its clean installed audit.

### 2026-10-02 — inline optional application settings

- The Create application form now uses an inline **Optional settings** disclosure in place of the vague **Configure more…** action. SDK, build and emulator inputs remain in the form, and Review stays visible. Legacy Tk and wx fallback buttons use the same label. A catalog-driven JavaScript render check verified those groups and the Review action; 22 focused Pytests and JavaScript syntax passed. The rebuilt macOS wheel passed its installed-wheel audit.

### 2026-10-02 — selected firmware inspection pane

- Firmware inspection is now driven by catalog selection instead of a separate action card. The catalog occupies the left side and the selected identity's device details, searchable manifest, provenance and raw JSON occupy the right. Inspection is read-only, runs in the background and caches each identity's result. Source settings remain accessible beneath the catalog. The raw JSON viewer now highlights only JSON, with no unrelated language selector.
- A hidden WKWebView check loaded seven imported identities, selected E6-00 automatically, indexed 13,186 files and measured the two pane columns at 323 and 611 pixels. The old Inspect action and language selector were absent. Twenty-two console Pytests, JavaScript syntax and the rebuilt installed-wheel audit passed. No phone protocol operation was run.

### 2026-10-02 — live selection and faster device status

- The Current selection sidebar now offers folder pickers for the working directory, application and SDK, plus a connected-phone selector. It shows full effective paths and updates automatically. The chosen working directory is passed to isolated CLI children; project and SDK selections feed context-aware form defaults. Clearing a selected phone suppresses automatic re-selection.
- A serial-free native USB topology observation runs every 750 ms, independently of the SDK command worker. On a changed topology it requests authoritative context reconciliation; if the selected vendor/product disappears, it suppresses the idle green status immediately while keeping pending work amber. A simulated removal in hidden WKWebView cleared the green status and displayed “Checking USB connection…” before the slower context request returned. A second live check observed the status sequence Refreshing firmware → Reading firmware details → Ready. Fifty-nine console, CLI and device Pytests, Ruff, Black and JavaScript syntax passed. No physical disconnect was performed.

### 2026-10-02 — console application selection

- The application picker and context discovery now accept `symbian.toml` as well as `symbian-project.json`. This permits selecting the checked-in `examples/gui_app` source project and keeps generated/legacy JSON projects selectable. JSON-only SDK settings are read only when that file exists. A focused bridge test selected `gui_app` and a JSON-only fixture; 59 console, CLI and device tests passed with Black and Ruff checks. The rebuilt wheel passed its installed-wheel audit, and the packaged manifest helper recognized `gui_app`.

### 2026-10-02 — C++20 app code and compact device cards

- `gui_app/app.cc` now directly uses `std::thread` and `std::make_shared` to run its existing Window Server loop on one owned event thread and return its result. The historical SDK adapter moved to `window_server.cc` because its placement-new declarations conflict with libc++ headers. The event thread creates its own cleanup stack; visual behavior and the stackless timer stay the same. The published E32 build was reproducible, the host GUI model test passed, the two preserved RM-807 GUI emulator replays passed, and all four opt-in debugger cases passed.
- Console device cards now reserve a small image column beside the device name, USB identity and interfaces. An attributed transparent Nokia 808 photograph appears only for identified 808 PureViews; other supported models use the generic SVG. The image is bundled locally with the WebKit page. A JavaScript render check confirmed one 808 image and two generic fallbacks; 59 focused Python tests and the rebuilt installed-wheel audit passed.

### 2026-10-02 — selected application workspace and build repair

- `symbian console --workdir PATH` now starts the detached desktop process in that directory, selects a project there when it has `symbian.toml`, and prefers its local `.symbian/app-sdk/sdk.json`. The selected SDK manifest is also passed into isolated command children. The Applications sidebar shows a compact indented entry for the selected project, using a bounded, project-local manifest icon when available.
- The application view reads TOML identity, architecture, UID3, caption, package name and current executable. It offers Build, emulator Run with an imported-firmware selector, and Package after an executable exists. Standalone TOML projects now use the shared emulator launcher with their selected source directory and SDK. The launcher remains responsible for ownership and stopping the emulator process.
- A `gui_app` build from the application directory initially failed only the independent ELF equality check: the two converted E32 executables had the same SHA-256, but DWARF `DW_AT_comp_dir` contained different nested CMake paths. Stable compilation directories in the GUI and generated-project CMake flags repaired the check without relaxing it. Fresh `gui_app` and generated-project builds returned `reproducible: true`, packaging produced `gui_app.sis`, and a standalone `session(project=gui_app)` reached READY and closed cleanly on imported RM-807 firmware. Twenty-seven focused Console tests passed, including a JavaScript initial-render regression check; this session check proves emulator startup, not a full UI interaction replay.

### 2026-10-02 — application and device view refinements

- An explicitly supplied `symbian console --workdir` now opens the selected application view when its TOML identity validates; the first paint shows a small loading state until context and project details arrive. On macOS, the launcher finds the invoking terminal's window through its process ancestry and places the detached WebKit window on that display. A local ancestry probe selected display 1 for the current terminal.
- The application view now uses the declared caption as its heading, with identity facts in one compact card. The build control uses a spanner icon, live tool output is copied to a private bounded-read log, and CMake compile groups appear as one compact disclosure instead of nested cards. A real reproducible `gui_app` build returned JSON success and wrote 334,031 bytes of tool output to the live log. The log integration test observed its first line while the child process was still running.
- EKA1 firmware entries are disabled for an EKA2 application run. Console Run requests a foreground emulator window and passes the console display index; the maintained EKA2L1 patch positions and activates that window. The patched emulator target rebuilt successfully. A direct foreground `gui_app` run opened its EKA2L1 window on display 1 at CoreGraphics bounds `(-58, -1035, 902, 725)` and reported it on screen; the supervisor was deliberately stopped after observing the window. The USB interface result view now uses short expandable rows, while firmware browsing defaults to a selected connected phone's matching model and retains a manual firmware choice. The Nokia 808 portrait was replaced with a transparent crop of the image supplied by the user. Thirty focused Console tests, JavaScript syntax, Black and Ruff passed. Window stacking above an actual console and cross-platform monitor behavior still need a human visual check.
- While a build runs, its tool log is expanded. The same log becomes a collapsed disclosure as soon as the result arrives, leaving the concise Build complete card prominent; the full log remains available on demand. The final focused suite still passed 30 tests.

### 2026-10-02 — GUI Run host debugger repair

- The IDE's Debug action on GUI Run failed before launch because it selected a nonexistent bundled GDB for macOS. The local IDE generator now supplies a host LLDB debug profile and selects it when generating GUI Run settings; the separate ARM GDB profile remains for GUI Debug. Both ignored `.idea` project windows were regenerated with the new profile.
- `/usr/bin/lldb` launched the native `gui_app_run` launcher, stopped at `main` (`launcher.cc:10`) and showed a source backtrace; the process was killed before starting the emulator. The ARM GDB wrapper reported GNU GDB 17.2. Two focused IDE configuration tests, Ruff and Black passed. The still-open IDE retained its former GDB selection in memory after the XML update, as the owner's next screenshot confirmed. Select GUI Host LLDB in its debug-profile toolbar or reopen the project before using Debug. The ARM guest requires GUI Debug with Symbian GUI GDB selected.

### 2026-10-02 — live application Run output

- The selected application's Run action now expands a live output panel during build, launch and emulator execution. The console captures SDK tool and launcher diagnostics incrementally and reads a bounded tail of the owned emulator's `frontend.log`; the panel collapses after the run and remains available. The session log path is passed through a private sidecar and checked against the selected project's run directory before reading.
- A real `gui_app` emulator run showed tool output before completion and 11,807 bytes of frontend output while the process was still active; the combined console reader returned both. That verification run was stopped after the log check. Thirty-three console/frontend tests, Ruff, Black and JavaScript syntax checks passed.

### 2026-10-02 — IDE debugger selection state corrected

- The owner's next GUI Run Debug attempt still chose the nonexistent bundled GDB even after restarting the IDE. Inspection of the installed CLion 2026.2 plugin showed that its active debugger is persisted in `SelectedDebugProfileService`, keyed by run configuration and CMake profile. The prior generator had written an obsolete `CurrentDebugProfile` component while the actual GUI Run key still pointed to an empty non-shared GDB profile.
- The generator now replaces only GUI Run's active selection with the host LLDB profile ID in the actual service state, preserving other targets' choices. Both ignored IDE project workspaces were regenerated and show GUI Run mapped to the installed LLDB profile. Focused configuration tests cover an existing broken GDB mapping and unrelated debugger preservation. An already-open IDE can retain service state in memory; its live Debug action still needs a reload or toolbar selection to confirm the repaired mapping.
- A subsequent live Debug attempt still used `.symbian/clion-setup/gui-gdb` for `GUI Run`, then rejected the macOS `gui_app_run` executable as an unknown format. That confirms the still-open IDE kept its old debugger choice; it does not indicate a malformed host executable. `file` identifies the target as arm64 Mach-O, and `/usr/bin/lldb` launched it, stopped at `launcher.cc:10` and showed a source backtrace. The saved root and dedicated `SelectedDebugProfileService` entries both resolve `GUI Run` to `/usr/bin/lldb`; an IDE project reload or explicit GUI Host LLDB selection is required to replace the live cached choice.
- With **GUI Host LLDB** visibly selected, a live IDE Debug session started and paused at `_dyld_start` after the launcher `exec` into Python. The IDE log recorded its LLDB frontend, and the traced Python supervisor was present. The owner clicked **Resume Program** in the Debug tool window and confirmed that the emulator ran. This is LLDB's default stop-on-exec behavior, not a launch failure. Guest source debugging remains the separate **GUI Debug** / **Symbian GUI GDB** path.
- The owner confirmed the missed breakpoint was in Symbian application C++ while the selected configuration was **GUI Run** with host LLDB. The generated IDE state now also binds **GUI Debug** to the ARM GDB profile in both root and standalone projects. The backend was exercised on the current GUI build: `GuiMain` and `DrawGui` source breakpoints hit, and the GDB machine interface relocated a breakpoint set before remote connection, then stopped and stepped in guest code. Four focused configuration/live tests passed; their old literal instruction-address expectations were replaced with assertions against the current build's breakpoint addresses. The owner selected **GUI Debug** / **Symbian GUI GDB** and confirmed the live IDE stopped at the guest breakpoint. The IDE log recorded `GUI Debug` launching the generated GDB wrapper in MI mode; full stack unwinding remains unverified.
### 2026-10-04 — A11 channel interface correction

- Host `<thread/channel.h>` now uses the pinned A11 header and waiter state.
  The guest `<thread/channel.h>` has A11's public `Reader`, `Writer` and
  `Channel` signatures, including selectable read/write cases,
  `WriteUnlessCancelled`, `length()` and zero-capacity rendezvous.
  `EventMailbox` instead uses `symbian::concurrency::BoundedChannel` for its
  fallible, nonblocking enqueue and idempotent shutdown behavior.
- The focused host concurrency test passed, and the guest channel probe passed
  all eight ARMv5T/ARMv6 × Dyncom/Dynarmic normal/changed cases, covering
  buffered transfer, rendezvous, competing cases, timeout, cancellation and
  losing-case ownership. The A11 source pin check verified 46 sources and 92
  includes. A fresh `.symbian/a11-channel-sdk-final-20261004` export contains
  matching guest headers, both ARM fiber archives and verified digest entries;
  the full eight-case guest matrix passed against that export. The rebuilt
  host channel and fiber tests passed 2/2. Full A11 concurrency parity is still
  open: fiber trees, joining, shared pool scheduling, structured futures and
  original test mapping.

### 2026-10-04 — component device API foundation

- Added `cpp/symbian/api/` with separate source boundaries for system,
  connectivity, power, media, display, sensors, camera and storage. Each
  implemented component is an independent opt-in `Symbian::<Component>`
  archive; the SDK does not create targets for planned components without an
  archive. The API rules and verification sequence are in `docs/DEVICE_API.md`.
- The first implemented component, `Symbian::System`, reads native tick and
  fast counters with their reported period or frequency and returns typed
  `absl::StatusOr` results. A host GTest passed native-error and zero-metadata
  cases. The packaged API probe passed all eight ARMv5T/ARMv6 ×
  Dyncom/Dynarmic normal/changed emulator cases. The fresh SDK export at
  `.symbian/device-components-sdk-20261004` contains both archives, its public
  header and verified digest entries.
- At this checkpoint, connectivity, power, media, display, sensors, camera
  and storage have component directories and plans only. Their native
  contracts and device behavior remain unverified; no physical-device API
  test was performed.

### 2026-10-04 — HAL and File Server device components

- Added separate `Symbian::Power`, `Symbian::Display` and `Symbian::Storage`
  archives and modern public headers. Power reports independently optional
  power-good, external-supply and qualitative battery fields; it never guesses
  a percentage or charging state. Display reports positive primary HAL pixel
  geometry with optional twips. Both keep legacy HAL headers behind native
  bridge translation units.
- Storage provides move-only read-only file and directory owners. Reads write
  directly into a caller span and directory entries stream one at a time.
  Each owner closes its File Server session and subsession; its lifetime stays
  on the opening worker thread. File offsets beyond the initial 2 GiB profile,
  writes, subscriptions and cross-thread session ownership are not claimed.
- The fresh `.symbian/device-api-final-sdk-20261004` export packages the four
  component archives for ARMv5T and ARMv6, HAL/File Server proxies and public
  headers. Seventeen checked assets match SDK digests; all four public headers
  match source. The host device API GTest passed. Its packaged guest probe
  ran successfully in all eight ARM architecture × Dyncom/Dynarmic ×
  normal/changed event-executor cases; the changed control belongs to the
  event executor, not the new device APIs.
  On the RM-807 emulator fixture, a read of `Z:\sys\bin\euser.dll` was denied
  by the platform; a read of `Z:\resource\psui.r01` and a streamed resource
  directory entry succeeded. No physical Nokia 808 behavior is established.
- Connectivity, sensors and media remain planned rather than exported.
  Native service contracts, permissions, buffer ownership and cancellation
  still need their own evidence before exposing those APIs.

### 2026-10-04 — explicit storage writes and incremental copying

- `Symbian::Storage` now adds a move-only `WritableFile` with explicit
  create/open/replace modes, positional writes from caller memory, and File
  Server `Flush`. `CreateDirectories` is a separate operation. The public
  header does not require `RFs`, `RFile`, `RDir`, descriptors or `User::`.
  Original platform headers and import proxies remain available to an
  application that explicitly opts into native API translation units.
- `FileCopy` owns one reusable 32 KiB buffer. `Step()` performs at most one
  read and write and returns partial progress; `Cancel()` atomically requests
  a stop before a subsequent request. `DirectoryReader::Next()` yields one
  entry per call, and its `Cancel()` stops at the next checkpoint. This bounds
  work and memory *per step*, not the
  wall-clock latency of an in-flight synchronous File Server request.
  Verified native asynchronous cancellation and drainage remain open work.
- Eight host device API GTests passed, including explicit replacement,
  progressive copy and cancellation before another chunk. A packaged guest
  probe created and flushed a private `C:` file, read it back and copied it
  through `FileCopy`; all eight ARMv5T/ARMv6 × Dyncom/Dynarmic ×
  normal/changed executor cases passed in disposable emulator copies. This
  does not establish physical-device write behavior or hardware durability.
- The candidate connectivity monitor remains unexported: the prepared client
  header/import contract is missing, and the emulator server currently
  reports a fixed connection count and bearer rather than observed state.
- Final export `.symbian/device-api-final-stream-sdk-20261004` packages both
  storage archives with the allocation-before-replacement correction. All
  3,271 SDK asset digests matched; a fresh ARMv5T/Dyncom guest probe passed
  with that exact export. The full eight-case matrix passed immediately before
  the allocation-order-only correction.
- The subsequent `.symbian/device-api-cancel-sdk-20261004` export adds
  `DirectoryReader::Cancel()` to the packaged public header and archives.
  Its 3,271 asset digests matched, the packaged storage header matched source,
  and a fresh ARMv5T/Dyncom guest probe passed.

### 2026-10-04 — SD analysis and camera discovery

- Root `SD_STORAGE.md` records software opportunities for SD filesystem
  throughput: fewer larger I/O calls, drive-reported buffer hints, deliberate
  cache modes, pre-sizing and flush placement. These are hypotheses pending
  controlled measurements on a physical removable card; the current 32 KiB
  copy step stays as a responsive default.
- `Symbian::Camera` exports `DiscoverCameras` with a typed count and explicit
  native errors. It does not reserve, power on or capture from a camera.
- Nine host device API GTests passed. The camera SDK export at
  `.symbian/device-api-camera-sdk-20261004` packages ARMv5T and ARMv6
  archives, ECam headers and the `CamerasAvailable` import stub. All 3,307
  packaged asset digests matched. A packaged
  ARMv5T/Dyncom guest probe passed with runtime discovery. This first export
  does not establish physical camera or SD throughput behavior.
- A candidate `InspectCamera` was removed before export: its `CCamera::New2L`
  leave-catching path first produced unresolved `TTrap` symbols under the
  wrong leave mode. The exception-based ARM EABI mode linked after adding
  frozen `drtaeabi` imports, but ELF-to-E32 conversion then rejected its
  imported C++ type-info vtable because that path only accepts function
  imports. Opening without a proven converted leave boundary would be unsafe.

### 2026-10-04 — development service design

- Root `DEVELOPMENT_AGENT.md` defines a staged on-device service and host
  integration for USB/Wi-Fi discovery, authenticated sessions, scoped file
  transfer, logs, process control, screen, ordinary app deployment and user
  process debugging. It is a design, not a running service. Its resident idle
  behavior and actual USB/WLAN capability need on-phone measurement.
- Recovery and flashing are explicitly absent from the service protocol and
  remain under the separate human-governed broker required by `PLAN.md`.
- An A11 reference pass refined the protocol design: keep `WireStream`
  lifecycle and `ChunkStoreReader`/`ChunkStoreWriter` backpressure concepts,
  but start with a one-session, 64 KiB frame and 256 KiB inbound phone profile.
  HTTP remains a host-side console API rather than a phone-side dependency.
- The plan now has an explicit emulator-to-development-phone gate: a verified
  ordinary SIS and read-only authenticated handshake precede manual install,
  on-phone idle/recovery checks and later boot-start enablement. This gate has
  not been executed.
