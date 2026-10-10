# Transition to one shared SDK runtime

Status: implementation plan. The shared SDK runtime described here is not yet
built or verified. Keep static linking available throughout the transition.

## 1. Goals and scope

- Install the common SDK implementation once instead of embedding it in every
  application SIS.
- Reduce repeated code loading and relocation work where the device loader can
  reuse code. Measure cold and warm launches separately; DLL use alone does not
  establish a launch-time improvement.
- Provide one SDK runtime DLL per selected runtime identity. Do not create
  separate libc++, Abseil, allocator, or SDK component runtime DLLs.
- Allow multiple runtime releases to be installed and used concurrently by
  different applications.
- Preserve a supported static mode that needs no installed SDK runtime package.
- Keep application source and public SDK APIs independent of the linkage mode.
  Preserve composition, ownership-oriented factories, valid moved-from states,
  callable wrappers, and the boundary excluding Symbian headers from public SDK
  headers.

This changes deployment and binary ownership. It does not expand the supported
C++ language/runtime contract: ordinary application exceptions remain disabled,
errors use Status/StatusOr, and existing TLS, locale and POSIX limitations remain
until separately implemented and verified.

## 2. Current evidence and gaps

The failure-handler example has an 822,672-byte E32 executable and an
825,156-byte uncompressed signed SIS. Approximate symbol attribution identifies
353 KB of libc++, 278 KB of Abseil and 81 KB of mimalloc. These are symbol-based
estimates, not an exact archive contribution map or a prediction of the final
shared-runtime size. ELF debug information is already excluded from the E32.

Existing infrastructure provides:

- E32 DLL conversion, writable data/BSS relocation, ordinal import libraries,
  and bounded constructor/destructor probes.
- `symbian_add_dynamic_library`, optional frozen export definitions, and
  application packaging with project DLLs bundled or supplied externally.
- Static SDK runtime profiles, matching configured libc++ headers, automatic
  failure handling, and guest checks on 808, C7, E6, 6120 and E71 ROM fixtures.

Work still required includes owned data exports, a runtime-only SIS package,
SIS dependency declarations, binary ABI identities, and comprehensive runtime
state/lifetime checks across DLL boundaries. The current DLL helper hides
runtime archive symbols and automatically selects a static runtime; publishing
an ordinary project DLL is therefore insufficient to build this provider.
Current E32 conversion also writes a fixed module version, which must become
an explicit checked input.

General DLL teardown ordering and DLL TLS destruction remain unresolved. ARM
ELF generation, E32 conversion, and structural inspection do not prove loader
compatibility or physical-device installation.

## 3. Binary ownership

| Location | Responsibilities |
| --- | --- |
| Shared runtime DLL | Supported out-of-line libc++, Abseil, allocator, C/C++ ABI adapters, A11 thread/concurrency implementation, and common SDK API implementations, including logging and failure reporting |
| Application image | Application logic, necessary template/inline instantiations, and a small SDK-owned entry/termination bridge |
| Application-owned DLLs | Application implementation, importing the same runtime identity as their executable |
| Original platform DLLs | Their existing frozen ABI and native services; they remain OS dependencies |
| Optional third-party libraries | Remain statically linkable unless a separate existing platform deployment contract applies; they must not embed a second SDK runtime |

Public component targets such as Display, Storage and System remain granular
source/build interfaces. In shared mode their common implementations resolve
through the same runtime import library. Applications do not gain responsibility
for selecting export symbols or writing native loading code.

Do not make the common DLL eagerly import optional camera, MIDI, tactile, GPU,
USB, or newer OS libraries that would prevent an otherwise compatible app from
starting. Keep optional native services behind implementation-owned lazy
bindings and return a typed availability error when used. Audit the complete
transitive OS import closure before adding a component to the common DLL.
An optional implementation that cannot yet satisfy this rule may remain a
statically linked SDK component, using the selected runtime for shared state.
It must not become another SDK runtime DLL.

Retain tiny image-local compiler helpers where relocation or ABI rules require
them. Such helpers must not own a second allocator, logging registry, scheduler,
locale registry, or process finalization registry.

## 4. Runtime identity and side-by-side releases

Distinguish these identifiers:

| Identifier | Meaning |
| --- | --- |
| SDK release | Host tools, headers, libraries and documentation release |
| ABI epoch | Compatibility contract for types, calls, symbols, ordinals and runtime behavior at binary boundaries |
| Platform baseline | Required ISA, EABI settings, OS import contracts and supported runtime profile |
| Runtime release | Immutable implementation release within an ABI epoch and baseline |
| Artifact digest | Exact bytes of the DLL, import library, headers/configuration and package |

Illustrative DLL names are `sdrt_a1_b1_r1.dll`, `sdrt_a1_b1_r2.dll` and
`sdrt_a2_b1_r1.dll`. Final names must pass actual E32 filename/import constraints
and case-insensitive uniqueness checks. Do not allocate production UIDs in this
plan.

Each identity has:

- A unique installed DLL filename, DLL UID3 and SIS package UID.
- An explicit E32 module version and SIS version, with checked mappings from
  release metadata to the format's representable version fields.
- Its own frozen ordinal definition and generated import library.
- A manifest recording the ABI epoch, baseline, release, dependency identities,
  capability set, configured headers/toolchain, symbol contract and digests.

Applications import the exact installed filename recorded by their SDK lock.
Their SIS declares the matching runtime package UID and required version using
the installer-supported dependency semantics. The unique package UID and exact
DLL name provide identity even where the installer only supports minimum-version
dependencies. Do not implement a mutable `latest.dll` alias or resolve a runtime
by installation drive order.

### Release and update rules

1. Publish every runtime release under a new identity, even for a compatible
   patch. Never replace the bytes of a released runtime identity.
2. Preserve ordinals for compatible releases: append exports, retain holes and
   never reuse a removed ordinal. SDK import libraries remain reproducible.
3. An incompatible change creates a new ABI epoch. Examples include changed
   exposed layouts, incompatible libc++ configuration, changed calling
   conventions, or an Abseil upgrade without demonstrated compatibility.
4. Existing applications keep using their locked release. Rebuild/repackage an
   application to move it to a newer release; this also permits deliberate
   rollback without changing another application's dependency.
5. Install releases alongside each other. An update or uninstall of one runtime
   package must leave other releases and their files intact.
6. Use installer dependency records to protect in-use releases from removal.
   SDK deployment tools additionally show reverse dependencies. Unknown or
   unmanaged consumers prevent automatic garbage collection; no background
   cleanup service or automatic deletion of runtime DLLs is required.
7. Do not hot-swap a running process's runtime. Respect installer file-in-use
   behavior and keep old packages available until their dependents have moved.

Multiple identities may coexist on the device and execute in different
processes. One process must use one identity across its executable and every
SDK-owned/application-owned DLL, including plugins. Build and deployment checks
reject known mismatches rather than silently selecting a second runtime.
Loading multiple SDK runtimes into one process is outside the initial contract.
Foreign libraries require an explicit audited ownership/ABI boundary; build
metadata cannot prove the behavior of arbitrary external binaries.

In static mode, the application image owns its runtime. An independently
statically linked DLL cannot exchange owning C++ objects with that image as
though they shared allocator and library state. Keep such a module isolated
behind an audited boundary with module-owned destruction, or select shared mode
for the connected module graph. Do not silently inject a common runtime
dependency into a supposedly standalone static application.

## 5. ABI contract and exports

Generate an ABI manifest from the actual configured build. Record at least:

- Target ISA, ARM EABI, endianness, soft-float convention, pointer width,
  `wchar_t` width, alignment/packing and applicable compiler ABI options.
- Pinned libc++/compiler-rt and Abseil revisions, libc++ ABI namespace/version,
  feature configuration and hardening settings.
- Exception, native-leave, RTTI and visibility boundaries; allocator and thread
  configuration; supported public data/type layouts.
- Function and data exports, mangled names, ordinals, and image-local symbols
  which must never be exported or coalesced.

Abseil's release namespace is not a compatibility guarantee. Validate each
upgrade and change the SDK ABI epoch whenever compatibility cannot be preserved.
Do not assume that changing an inline namespace alone makes two runtimes safe
to mix in a process.

Add an SDK-owned export-generation policy for runtime builds. Root the complete
supported binary surface, hide implementation details, and freeze the resulting
ordinal contract. Do not rely on wildcard export discovery or export unsupported
upstream functionality simply because it exists in an archive.

Implement and test owned data exports/imports needed by the supported C++ ABI,
including standard stream objects, `std::nothrow`, vtables and type information
where applicable. SDK APIs under our control may use function accessors for
state, but standard library object contracts must remain correct. Preserve
per-image entities such as `__dso_handle`. A missing data relocation form must
fail conversion instead of being silently copied into the executable.

Template instantiations and inline code may remain in applications. Their ABI
configuration must match the runtime, and their allocations and shared state
must resolve to it. Link-map audits must catch accidental static runtime copies,
including copies pulled through third-party archives or whole-archive flags.

## 6. Allocation, initialization and shutdown

The shared code serves many processes; each process owns its own writable
runtime state. No runtime daemon, cross-process application heap, or manually
managed application runtime session is introduced.

- Route global new/delete, aligned and nothrow forms, C allocation functions
  supplied by this runtime, and relevant library allocation paths to one
  per-process allocator. Ordinary allocation failure and nothrow semantics
  remain unchanged.
- Allocate through one module and destroy through another only when both use
  this runtime and the same ownership contract. Objects whose implementation
  or callbacks live in an application DLL must not outlive that DLL.
- Establish native heap/thread prerequisites before runtime allocation or
  constructors. Define a minimal bootstrap path which does not call back into
  an uninitialized allocator, logger or C++ library.
- Use lazy process-owned initialization for services that need OS resources.
  Keep heavy work out of DLL attach and avoid depending on incidental DLL
  constructor order.
- Use A11's thread facilities and the existing guest backend. Worker threads
  must use the correct process allocator state; do not introduce a second
  scheduler or duplicated thread-local registries.
- Register logging and failure handling once per process. The application entry
  bridge connects Main/Status, CHECK, terminate and fatal runtime paths to the
  same handler. Background failures persist logs without opening a foreground
  UI. Remove callbacks/sinks before their owning module can disappear.
- Define process finalization and per-image finalizers explicitly. Drain owned
  asynchronous work, stop workers, run required application finalizers and only
  then release runtime resources they may access. Test actual ordering rather
  than assuming process termination makes every teardown safe.
- Keep the runtime loaded for the process lifetime through normal imports and
  SDK-managed module ownership. Applications do not call Load/Unload or provide
  runtime initialization handles.

Retaining the runtime does not solve arbitrary plugin unload ordering or DLL
TLS destruction. Add dedicated support/tests before advertising those features.

## 7. Build and SDK structure

Introduce one project-level linkage selection, illustratively:

```cmake
set(SYMBIAN_SDK_LINKAGE shared CACHE STRING "SDK linkage: shared or static")
find_package(SymbianSDK CONFIG REQUIRED)
target_link_libraries(my_app PRIVATE Symbian::Runtime Symbian::Display)
```

The exact spelling is to be settled against the existing SDK CMake surface.
The intended behavior is:

- `shared`: Runtime and component facades resolve through the locked common
  runtime import library, plus necessary local bootstrap/optional code.
- `static`: the same component facades resolve to static archives, retaining
  dead-section elimination and requiring no shared runtime package.
- A single dependency graph cannot select conflicting modes, profiles, ABI
  epochs or identities. Diagnostics identify the dependency introducing a
  conflict. Original firmware DLL dependencies are not duplicate SDK runtimes.
- Build the runtime provider from reusable implementation targets with a
  dedicated provider mode. It must not automatically import itself or root a
  second static runtime through the existing default-runtime selection.
- Keep link mode and identity explicit in exported targets, SDK manifests,
  project locks, linker diagnostics and packaging results.
- Ship static archives, shared import libraries, ABI manifests, debug symbols,
  licenses and matching headers in the host SDK. Keep artifacts for each
  identity separately addressable.
- Retain existing static projects/locks unchanged. Start shared mode as an
  opt-in; switch newly generated project defaults only after acceptance gates.
  Preserve static mode as a supported release/test configuration afterward.

Evaluate a common older-compatible ARM baseline first. If incompatible OS
imports or ISA requirements require multiple baselines, give them different
identities and select by documented feature/import contracts. Do not build
phone-specific runtimes or silently treat one model name as an ABI guarantee.

## 8. Packaging, installation and signing

Extend the native SIS implementation to support runtime-only DLL packages and
dependency declarations. Native code remains the owner of SIS format logic;
Python supplies policy and typed orchestration. Validate the new forms with
independent format/signature oracles and the actual installer.

- Install runtime DLLs to a fixed, always-available system-drive location under
  `sys/bin`, subject to verified installer/loader rules. Do not install the same
  identity with different bytes on multiple drives; USB mass-storage exposure
  or removable media must not disconnect a required runtime.
- A normal shared application SIS contains its own payload and runtime
  dependency metadata. Install the runtime once using its independent SIS.
- Offer offline distribution as application and runtime packages with a
  machine-readable deployment manifest. The host tool installs prerequisites
  in order; embedded SIS support is a separate optional format feature.
- Resolve prerequisites from the exact project lock, verify identity/digests,
  and stage or install missing packages through already supported transports.
  Do not silently rewrite application ABI requirements to match the phone.
- Check the complete DLL dependency capability rule: runtime DLL capabilities
  must satisfy their consumers, with a certificate/chain authorized for those
  capabilities. Capability declarations alone do not grant them. Include this
  check for every supported application capability set and OS baseline.
- Do not lower application capabilities to make a dependency load, embed
  phone-specific keys, or work around security policy with extra privileged
  services. An unavailable signing/deployment combination remains unsupported;
  static mode may serve applications where its real constraints are satisfied.
- Missing or incompatible runtimes should be detected before launch by host
  deployment tools. A loader failure can occur before application entry, so the
  failure-handler GUI cannot be the only diagnostic mechanism.

SIS compression remains useful in both modes and should be handled as a separate
packaging improvement. It reduces transfer size, not installed runtime size or
application code loading. Shared-runtime deployment does not depend on E32
compression support.

## 9. Implementation sequence and gates

| Phase | Deliverables | Gate before proceeding |
| --- | --- | --- |
| 0. Inventory | Exact link maps, import/state ownership inventory, baseline size/startup measurements, written ABI identity schema | Account for duplicated code and all runtime state/initialization roots |
| 1. DLL foundations | Owned data exports, frozen ordinals, explicit E32 versions, ABI metadata, provider build mode | Positive and negative relocation/export/loader tests on modern and older ROMs |
| 2. Minimal shared provider | One DLL with allocation, required libc++/Abseil subset and process bootstrap; two independent clients | Cross-image allocation/destruction, process isolation, constructors and normal/error shutdown work |
| 3. Runtime completeness | Supported common runtime/SDK surface, logging/failure hooks, threading and optional-native-service boundaries | No accidental second runtime; optional missing services do not break startup |
| 4. Distribution | Runtime-only signed SIS, dependency records, exact locks, installer/reverse-dependency reporting | Two releases coexist, apps bind correctly, missing/wrong dependencies fail clearly, uninstall respects dependents |
| 5. App migration | Build all examples and guest probes in shared and static modes | Behavior, ownership, header canaries and failure handling pass in both modes |
| 6. Release/default switch | Published SDK/runtime artifacts, measurements, upgrade/rollback guide | Physical Nokia 808 checks and older-ROM gates pass; measured results justify the shared default |

The minimal provider is an implementation milestone, not a separate runtime
product that applications must manage. Every phase preserves a working static
build and keeps unsupported behavior visible.

## 10. Verification matrix

### Functional and ABI checks

- Build headers independently at host/guest/Python boundaries and run the owned
  C++ style checks. Keep public SDK headers free of transitive Symbian headers.
- Test process-private globals, locale/log state and heaps using two concurrent
  applications importing the same runtime.
- Exercise strings, containers, Status/StatusOr, std::function/AnyInvocable,
  smart-pointer deleters, standard streams and aligned/nothrow allocation across
  executable/DLL boundaries. Destroy them in the opposite module where their
  contracts allow it.
- Exercise worker creation/exit, timer cancel-and-drain, suspended/background
  apps, module-owned callbacks, and teardown while work is outstanding.
- Exercise CHECK/log sinks, fatal foreground GUI, background report persistence,
  Copy/Exit, and early bootstrap failures with bounded fallback diagnostics.
- Reject wrong ABI metadata, changed/reused ordinals, missing data exports,
  malformed dependencies, unsigned/unacceptable capabilities and mixed static/
  shared runtime ownership. Verify actual loader rejection where relevant.
- Build an old client, preserve its bytes, publish another runtime release and
  prove the old client still runs. Run clients of different releases
  concurrently. Remove/update one release without disturbing another.
- Test static apps with no SDK runtime installed and shared apps with their
  locked runtime. Do not accept a test that passes only because an unintended
  runtime copy is already installed.

Use both emulator CPU backends for lifecycle/loader probes and the preserved
808, C7, E6, 6120 and E71 fixtures for reached functionality. Record fixture
identities and explicit skips. Keep EKA1 outside the initial acceptance claim.
Verify installation, signing, real launch and shutdown on the physical Nokia
808; emulator results do not establish physical compatibility on older phones.

### Size and startup measurements

Compare identical source/configuration in static and shared modes, including
Failure Handler Demo, a minimal no-UI app, a simple GUI app, SDL, camera and
Development Agent. Record:

- Signed application SIS size, prerequisite SIS size, installed bytes and total
  cost for one, two and several applications.
- Application text/data/BSS, runtime text/data/BSS, relocations, import counts,
  private RAM and actually shared code where measurable.
- Process creation to Main, first visible frame and usable first interaction;
  runtime initialization/relocation time separately where instrumentation permits.
- Repeated cold and warm launches, and launching a second app while the first
  uses the runtime. Report medians, tail latency, OS/firmware, storage and
  confidence in cache state. Do not label an unverified cache flush a cold boot.

A monolithic exported DLL can be larger than the statically retained subset,
and initialization/import overhead can offset code-cache savings. Keep optional
services lazy, measure export-induced retention, and evaluate size optimization/
LTO without changing ABI semantics. Set release thresholds from phase 0 data.
Do not claim faster launches until physical measurements show them; do not
switch defaults if critical workloads regress without an addressed cause.

## 11. Open decisions to resolve through evidence

1. Can one supported older-compatible OS/ISA baseline serve the initial devices
   without sacrificing required features or launch performance?
2. Which standard data exports and relocation forms are required by the exact
   supported libc++/Abseil configuration, and which can remain image-local?
3. What bootstrap/finalization ordering do the real loader and native heap APIs
   provide on each baseline, including a worker being the first runtime caller?
4. Which capability/signing sets can distribute one common runtime for the
   intended applications on unmodified devices?
5. Which installer dependency/uninstall semantics are actually enforced on the
   older and modern ROMs, and how are in-use runtime files handled?
6. What fraction of the current binaries disappears from each client, and does
   cold/warm loading improve enough to select shared mode by default?

Resolve these in the private research log with reproducible artifacts, then
update this plan and the public runtime/deployment documentation as gates pass.
