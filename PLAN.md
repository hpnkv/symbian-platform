# Mission: Revive the Nokia 808 / Symbian Belle Development Platform for 2026

You are working on a long-running engineering and research project whose goal is to create a **modern, macOS-native development stack for the Nokia 808 PureView and the Symbian Belle ecosystem**, suitable for serious application development, system-component development, reverse engineering, debugging, experimentation, and potentially eventually developing an alternative operating system for the hardware.

The goal is **not** to recreate Nokia's historical development environment verbatim.

The goal is to understand what the historical platform actually did, determine what is genuinely required to produce and execute software on the hardware, and then build the smallest, cleanest, most maintainable modern system that provides those capabilities.

Think of this project as:

> **"What would the Symbian/Nokia 808 development platform look like if it had survived and been redesigned for 2026?"**

rather than:

> "How do we reproduce Nokia's 2010 development environment?"


## Implementation review — 2026-10-01

The initial survey and several vertical slices are complete. Continue from this
checkpoint and [docs/STATUS.md](docs/STATUS.md); do not restart the initial
survey or infer that the platform mission is complete. The original long-term
objectives below remain in force. Current research and replay instructions are
in [docs/RESEARCH.md](docs/RESEARCH.md), [WALKTHROUGH.md](WALKTHROUGH.md),
[docs/BELLE_ABI.md](docs/BELLE_ABI.md) and
[docs/EMULATOR_CONTROL.md](docs/EMULATOR_CONTROL.md).

### What the work established

* Contemporary Clang/LLD, CMake/Ninja and a native E32/SIS implementation can
  replace the normal historical Windows build/package pipeline for the tested
  subset. Original Nokia checksums/whole-image validation and independent
  EKA2L1 loader/CPU/process/installer tests remain valuable behavioral oracles.
* C++20 language features, a named-module example and selected configured
  libc++ headers compile and run in the ROMless probes. The GUI also compiles
  as C++20 and runs with real SDK imports in the guarded firmware experiment.
  These results do not establish a hosted C++20 library or general target ABI.
* The supplied Delight v1.8 archive provides a usable preserved RM-807 ROM/Z
  fixture. Its metadata describes the archive; it does not identify the owner's
  phone, authenticate stock firmware or prove recovery suitability.
* The hardest GUI failures were runtime contracts: a firmware-specific
  executive-call map, a separate ARM TPIDRURO register and SDK cleanup-stack
  setup. No whole-table shift or replacement system-library implementation was
  sufficient evidence. The original emulator profile remains a control.
* Both tested macOS CPU backends now render the counter through real EUSER/WS32,
  accept pointer input, redraw after increment/reset, ignore an outside-control
  tap and exit with guest reason zero and frontend exit zero. Pixel oracles and
  native kernel exit records establish more than a drawing-function breakpoint.
* Live ARM GDB source/ROM breakpoints and stable instruction stepping work.
  Full unwinding, comprehensive crash/process inspection and CLion's debugger
  frontend remain unverified. CLion's GUI source belongs to a separate ARM
  CMake project; its standalone and machine-local presets and clangd context
  now pass checks. Symbian ARM settings are saved for the actual IDEA/CLion
  instance; the live preset uses Default with explicit ARM paths until those
  settings load. The actual IDE configures successfully and resolves all three
  GUI target sources; its remote-debug frontend remains an onboarding gate.

### What needs to change in the execution strategy

Keep the narrow, independently checked experiments, but consolidate them into
repeatable developer workflows before adding more platform breadth. The GUI now
has an owned foreground launcher and generated IDE Run/Remote Debug settings;
source builds, copied fixture, cleanup and post-connection symbol relocation
are checked. This remains a bounded example workflow, not the general lifecycle
manager or symbian test API. Ad hoc
private copies and GDB scripts proved contracts; they are not yet a general
emulator lifecycle API. Seven owned patches currently extend a pinned emulator
checkout. Keep their replay and regression coverage, document ownership and
seek upstreamable changes rather than allowing an unbounded local fork.

Separate four levels of evidence in reports: compiler/format acceptance,
ROMless loader/kernel execution, execution against a named preserved firmware
fixture, and physical-device compatibility. A success at one level must not
silently promote another. Emit fixture hashes and actual scope, including skips
and unresolved warnings. Reproducibility on this host is not a hermetic-build
attestation. Read-only file permissions are not immutable preservation against
the owner; the original archive still needs an independently held offline copy.

The current runtime still reports an unimplemented 0x10D loader operation.
SVC 0xFF interception, private executive operations and complete DLL initialization
are unresolved despite the GUI success. Writable data/BSS, TLS, general static
initialization, compiler runtime and hosted C/C++ libraries are not supported by
the current transport. Do not label the experimental toolchain a general Belle
SDK or make these failures disappear by dropping sections or ignoring errors.

### Next work, in priority order

1. **Finish developer onboarding and keep it exercised.** Maintain separate host
   and guest CMake contexts, correct compilation databases, SDK provenance,
   E32 publication after IDE edits and ARM debugger relocation instructions.
   The standalone and local GUI presets and clangd checks pass; actual toolchain
   settings are saved and the actual IDE target model resolves the GUI. Exercise
   the CLion debugger frontend and retain E32 publication coverage. The owned
   launcher and real GDB MI2 checks now pass; keep native debug-profile selection
   explicit in this IDE version. Document the application loop in WALKTHROUGH.md.
2. **Consolidate the emulator lifecycle and `symbian test`.** Build on the
   existing native capture/input/exit endpoint and independent installer tests.
   Add create/start/install/launch/log/stop/reset operations for explicitly owned
   disposable instances, with structured manifests, input/binary digests,
   bounded process ownership and retained artifacts. Stopped filesystem copies
   are the initial snapshot mechanism; do not claim live-machine snapshots.
   Acceptance: two fresh independent runs install the generated package,
   exercise the GUI, record PNGs/exit status, restore/discard the instance and
   leave every golden/input digest unchanged. Include startup failure, timeout,
   stale endpoint, wrong fixture and repeated-launch controls. Diagnose the
   observed intermittent frontend shutdown timeout with retained native stacks;
   repeated successful runs do not explain a timeout. Keep private runtime copies
   out of IDE indexing and add managed artifact retention.
3. **Close the demonstrated ABI/runtime gaps before expanding firmware scope.**
   Trace 0x10D and 0xFF with original source, actual wrapper/caller instructions
   and live guest observations. Test DLL initialization/destruction and repeated
   SDK cleanup, then focus/occlusion/orientation and resource lifetime. Retain
   exact-ROM opt-in guards and the unmodified/default profile control. Support
   another firmware only with its own independently checked contract and tests.
4. **Extend the target runtime through one bounded feature at a time.** Start
   with writable-data/BSS and relocations, then initialization/TLS and required
   compiler-rt/C-library entry points. Each feature needs original-validator
   acceptance, actual mapped execution, a failing control and cleanup checks.
   Only then build a configured no-exceptions C++ library subset with explicit
   allocation/failure policy. Containers, coroutines, atomics and threads are
   separate capabilities; enabling `-std=c++20` does not provide them.
5. **Make debugging failures useful.** Add correlated process/thread/module
   inspection, panic capture and source symbolication using real mapping data.
   Demonstrate a deliberate guest failure with a useful retained report, and
   establish an unwind contract before promising stack traces. Connect the
   tested ARM GDB path to CLion without assuming the UI is already validated.
6. **Expand application/system scope only after these gates pass.** Add resources
   and registration, then a minimal server/DLL/IPC slice and evaluate Avkon/Qt
   needs from real applications. Complete preservation and an enforced device
   broker before physical deployment; system replacement, hardware recovery and
   alternative-OS work retain their original human/device boundaries.

### Implementation constraints

Use A11 as the practical implementation reference: Python policy in `symbian/`,
native libraries in `cpp/symbian/<component>/`, bindings in `cpp/python/`, Google
style/docstrings, exceptions disabled with Abseil Status/StatusOr, and exceptions
only at pybind11 translation boundaries. Native format logic must remain native.
Use A11's thread library if adding native concurrency; existing emulator event
loops may be reused without introducing another scheduler. Async Python bindings
must follow A11's GIL and deferred reference-holder patterns. Synchronous parsers
and the socket policy client need no new callback/holder infrastructure.

Use GTest/Pytest, Black/Ruff at 80 columns, .clang-format, CMake presets/Ninja and
actual compile_commands.json. Keep upstream checkouts, firmware/private data,
builds and runtime artifacts ignored; preserve licenses. The GPL control adapter
belongs only to the research emulator, not the production Python extension.
The current full suite passes 170 Pytest cases with explicit optional inputs,
six root CTest targets and the new control/routing/register GTests. Keep evidence
counts in STATUS/RESEARCH_LOG current rather than treating these numbers as a
permanent acceptance threshold.

---

# 1. Core principles

These principles take precedence over convenience and should guide architectural decisions throughout the project.

## 1.1 Nothing in the historical platform is sacred

Do **not** assume that an existing Nokia/Symbian tool, compiler, build system, runtime, SDK component, file format implementation, emulator facility, compatibility layer, or historical dependency must be preserved merely because Nokia used it.

For every historical component, ask:

1. What problem did this component solve?
2. Why was it implemented that way?
3. What input/output or ABI contract does it actually provide?
4. Is that contract still necessary?
5. Can the underlying functionality be implemented more simply today?
6. Can a modern implementation eliminate an entire historical dependency?
7. Can the functionality be implemented natively on macOS?
8. Can the functionality be made substantially more observable, testable, deterministic, or maintainable?

Prefer understanding and rebuilding over blindly preserving.

A small amount of compatibility code around a genuinely necessary historical interface is acceptable.

A large historical runtime stack that exists merely because "that is how Symbian development worked" is not.

---

## 1.2 Minimise compatibility layers

Actively minimise:

* historical runtimes
* old interpreters
* compatibility shells
* Wine/Windows dependencies
* obsolete build systems
* ancient compiler toolchains
* duplicated SDK infrastructure
* historical GUI development environments
* proprietary utilities
* opaque binary tools
* unnecessary emulation

If a compatibility layer is proposed, document **why it is genuinely necessary**.

Do not introduce a compatibility layer merely because it is easier than understanding the underlying problem.

A native reimplementation is preferable when the functionality is sufficiently understood and the implementation is tractable.

---

## 1.3 The historical implementation is evidence, not architecture

Historical Symbian SDKs, binaries, source code, documentation, firmware and tools are valuable sources of information.

They are not automatically the architecture of the new system.

Use them as:

* specifications
* compatibility references
* behavioural oracles
* reverse-engineering targets
* sources of ABI information
* sources of historical knowledge

not as things that must necessarily be carried forward.

---

## 1.4 Prefer modern interfaces

The user is developing on macOS in 2026.

Normal workflows should therefore feel like modern software development:

* Git
* CMake where appropriate
* Ninja
* Clang/LLVM
* clangd
* LLDB/GDB where useful
* modern Python
* modern scripting
* reproducible builds
* deterministic tests
* structured logs
* symbolication
* source indexing
* automated emulator execution
* machine-readable diagnostics
* coding-agent integration

Do not make developers interact directly with historical tools unless there is a compelling technical reason.

---

# 2. The target platform

The initial target is specifically:

**Nokia 808 PureView, RM-807, Symbian Belle / Belle FP2-era software.**

The platform should ultimately support:

* ordinary Symbian applications
* Qt/S60 applications where useful
* native Symbian C++ applications
* system servers and DLLs
* plugins
* system components
* IPC-heavy components
* lower-level experimentation
* reverse engineering
* debugging
* potentially custom/alternative OS development

Do not assume all of these need to be supported simultaneously.

Build the infrastructure progressively.

---

# 3. macOS must be the canonical development environment

The normal development loop must run directly on macOS.

The project should not require a Windows VM for:

* editing
* normal compilation
* static analysis
* source navigation
* packaging
* emulator execution
* automated tests
* debugging
* reverse engineering
* ordinary application development

A developer should eventually be able to do things resembling:

```bash
symbian doctor
symbian build
symbian test
symbian package
symbian emu
symbian emu install foo.sis
symbian emu launch foo
symbian emu screenshot
symbian emu logs
symbian emu debug foo
```

without leaving macOS.

---

# 4. VMs are recovery tools, not development infrastructure

There should be **no general Windows VM dependency in the normal development stack**.

A VM may be retained for one narrowly defined purpose:

## Historical recovery

Maintain a sealed, reproducible VM containing whatever original Nokia/Symbian tools are genuinely required for operations that cannot reasonably be replaced.

Examples may include:

* Phoenix
* historical Nokia USB drivers
* historical firmware flashing utilities
* genuinely unreproducible proprietary tooling
* unusual device-recovery operations

The recovery VM should be treated as an appliance.

It should not become the place where ordinary development happens.

Do not add arbitrary development tools to it.

Do not make the normal build system depend on it.

Do not expose it as a general-purpose coding-agent environment.

---

# 5. Preserve the physical Nokia 808

Treat the physical phone as valuable hardware and assume unfinished software can damage it.

Before physical-device experimental development (read-only research and
disposable emulator work may proceed independently):

1. Identify the exact device variant.
2. Record its current software/firmware state.
3. Preserve all available firmware and ROM material.
4. Preserve relevant filesystem/device data where possible.
5. Hash preserved artifacts.
6. Document recovery procedures.
7. Establish a known-good baseline.

Create an independently held offline reference copy and record its digest.
Owner-reversible read-only permissions alone do not satisfy immutable preservation.

Prefer having a second Nokia 808 for experimentation if practical:

```text
808-A
    reference / preservation device
    normally never modified

808-B
    development device
    may run experimental applications/system components
```

The project should always retain a known-good recovery path independent of the continued health of the development phone.

---

# 6. Strict hardware safety boundary

The coding agent must not be given unrestricted control over the physical phone.

Divide hardware operations into tiers.

## Safe operations

Examples:

```text
device info
device logs
device screenshots
install ordinary application
uninstall ordinary application
run application
collect diagnostics
```

These may eventually be automated.

## Dangerous operations

Examples:

```text
reboot
modify system files
install system components
replace system DLLs
modify persistent system state
```

These require explicit human authorization.

## Recovery / irreversible operations

Examples:

```text
flash firmware
erase storage
modify bootloader
partition storage
modify OTP
modify calibration data
perform low-level recovery
```

These must **not be agent capabilities**.

The agent may prepare artifacts or instructions for such operations, but the actual operation must remain outside the agent's automated authority.

Do not rely solely on instructions in the agent prompt for this protection.

Enforce the boundary in the tooling itself.

---

# 7. EKA2L1 should be the primary experimental device

Investigate and use EKA2L1 as the principal Symbian emulator.

The emulator should become a first-class development target, not merely a convenient way to run an occasional application.

Build a macOS-native orchestration layer around it.

Desired interface:

```bash
symbian emu create 808
symbian emu start
symbian emu reset
symbian emu snapshot save clean
symbian emu snapshot restore clean
symbian emu install foo.sis
symbian emu uninstall foo
symbian emu launch foo
symbian emu stop foo
symbian emu screenshot
symbian emu logs
symbian emu trace
symbian emu dump
```

Investigate the emulator's actual capabilities before inventing additional infrastructure.

Where necessary, contribute improvements to the emulator rather than building external workarounds.

---

# 8. Emulator state must be disposable

Create a golden emulator state.

Tests should work approximately like:

```text
golden Belle image
       |
       v
disposable test instance
       |
       +-- install
       +-- execute
       +-- modify
       +-- crash
       +-- reboot
       +-- inspect
       |
       v
discard
```

A broken test must never poison subsequent tests.

The coding agent should be able to perform aggressive experiments in the emulator without fear of permanently corrupting the development environment.

Snapshots, filesystem copies, deterministic reset mechanisms and reproducible emulator images should be preferred over manual cleanup.

---

# 9. Build a modern Symbian platform facade

Create a repository that hides historical complexity behind modern interfaces.

Conceptually:

```text
symbian-2026/
    sdk/
    toolchain/
    build/
    packaging/
    emulator/
    debugger/
    analysis/
    device/
    tests/
    tools/
    docs/
    recovery/
```

The precise layout is yours to determine.

The important architectural distinction is:

```text
modern developer interface
          |
          v
platform implementation
          |
          v
historical ABI / hardware
```

not:

```text
developer
   |
   v
historical Nokia build environment
   |
   v
historical Nokia tools
```

---

# 10. Investigate the compiler/toolchain rather than assuming it

Determine what is actually necessary to produce valid Symbian binaries for the target.

Investigate, in order:

1. Modern LLVM/Clang capabilities.
2. Whether contemporary LLVM can generate the required ARM architecture and ABI.
3. Whether Symbian-specific ABI requirements can be supported directly.
4. Whether the old GCCE toolchain can be extracted and run natively on macOS.
5. Whether only a small part of the historical compiler infrastructure is actually needed.
6. Whether individual missing functions can be reimplemented.
7. Only then, if genuinely necessary, whether a historical executable needs a compatibility environment.

Do not preserve GCCE merely because it was historically used.

Conversely, do not rewrite it prematurely if it turns out to encode important ABI behaviour that is difficult to reproduce correctly.

The question is always:

> What exact technical property do we need from this tool?

---

# 11. Separate source intelligence from target compilation

Modern code intelligence should not depend on the historical compiler.

Aim for:

```text
                         source
                           |
                           v
                     clangd / LLVM
                           |
        +------------------+------------------+
        |                  |                  |
    diagnostics        navigation         indexing
    references         completion          analysis
    refactoring        call hierarchy      symbols
                           |
                           v
                    target compiler
                           |
                           v
                     Symbian binary
```

Provide accurate enough compilation databases and platform descriptions for clangd.

The developer should get modern:

* jump to definition
* find references
* completion
* diagnostics
* call hierarchy
* symbol search
* semantic highlighting
* static analysis
* refactoring support

even if the final target binary is produced by an ancient-compatible backend.

---

# 12. Rebuild historical utilities when practical

Investigate each historical utility individually.

For example, if an old utility converts:

```text
.pkg -> .sis
```

determine what it actually does.

If the functionality is well understood and feasible to implement:

```text
modern native implementation
```

is preferable to:

```text
Windows VM
    -> old Nokia executable
    -> output
```

The old executable can then become a compatibility oracle used in tests.

This principle applies to:

* package generation
* resource processing
* metadata generation
* symbol handling
* signing-related preparation
* build orchestration
* emulator configuration
* filesystem manipulation
* diagnostic extraction

Do not undertake unnecessary rewrites merely for ideological purity. The purpose is to reduce complexity and gain understanding.

---

# 13. Treat the historical SDK as research material

Do not blindly import the entire historical SDK.

For every SDK component, determine:

```text
What is this?
Why does it exist?
Who consumes it?
What contract does it provide?
Is that contract still needed?
Can we replace it?
Can we simplify it?
Can we expose it through a modern interface?
```

The desired outcome is not:

> "We have successfully reproduced the entire Nokia SDK."

The desired outcome is:

> "We understand which parts of the Nokia SDK are actually required, and we have the smallest maintainable implementation providing those capabilities."

---

# 14. Preserve compatibility where it has genuine value

There are cases where historical compatibility is itself important.

For example:

* existing Symbian binaries
* established Symbian ABI conventions
* Belle system APIs
* application package formats
* filesystem formats
* IPC protocols
* hardware interfaces
* binary layouts
* documented or de facto application behaviour

Preserve these where they are required to interoperate with the target.

But distinguish:

```text
compatibility required by the target
```

from:

```text
compatibility accidentally inherited from Nokia's development process
```

Only the former should automatically survive.

---

# 15. Build modern debugging and observability

A major goal is to provide a **2026-level debugging experience** even where the original platform did not.

Develop infrastructure for:

* symbols
* stack traces
* symbolication
* process inspection
* thread inspection
* memory inspection
* crash dumps
* logs
* tracing
* IPC tracing
* filesystem activity
* screenshots
* input capture
* deterministic reproduction
* emulator snapshots
* test artifacts

For example, an agent should eventually be able to receive something like:

```text
Process: MyServer.exe
Thread: WorkerThread
Panic: KERN-EXEC 3

Stack:
    MyServer::HandleMessage()
    MyServer::Dispatch()
    RMessage2::Complete()
    ...

Source:
    server.cpp:418

Registers:
    ...

Loaded modules:
    ...
```

rather than only:

```text
KERN-EXEC 3
```

---

# 16. Build an observability layer for new code

For components under our control, define modern diagnostic abstractions that can map to different backends.

For example:

```cpp
SYMBIAN_LOG("camera", "frame received");
SYMBIAN_TRACE_SCOPE("RequestHandler");
SYMBIAN_ASSERT(condition);
```

On the phone this might ultimately use historical facilities such as RDebug, files, serial output, or custom IPC.

In the emulator it could produce structured host-side events.

Do not prematurely imitate modern observability standards if the target makes that inappropriate. The important property is that diagnostics become machine-readable, correlated and useful to both humans and coding agents.

---

# 17. Make the development loop agent-friendly

The platform should expose operations as composable, machine-readable tools.

A coding agent should eventually be able to do:

```text
build
    |
    v
run unit tests
    |
    v
package
    |
    v
create clean emulator
    |
    v
install
    |
    v
launch
    |
    v
exercise UI
    |
    v
collect screenshot/log/crash
    |
    v
inspect
    |
    v
modify source
```

Avoid requiring agents to scrape terminal output or interact with opaque GUI tools where a structured interface can reasonably be provided.

Prefer JSON/structured output for tooling APIs.

---

# 18. Do not optimise for literal historical UI/API reproduction

The user wants to develop applications for the 808, not recreate every aspect of Carbide.c++, Nokia Developer Tools or the historical desktop experience.

It is fine—even preferable—to provide:

```text
VS Code / CLion / another modern editor
+
clangd
+
modern CLI
+
modern debugger
+
modern emulator UI
```

rather than reproducing historical IDE functionality.

Likewise, if a historical SDK GUI merely wrapped a straightforward command-line operation, replace it with a modern CLI/API.

---

# 19. Keep an explicit research log

For significant discoveries, record:

```text
Question
Historical behaviour
Evidence
Experiments
Conclusion
Implementation decision
Remaining uncertainty
```

This project will involve archaeology and reverse engineering.

Do not let assumptions become undocumented architecture.

If you discover that an apparently essential Symbian component is unnecessary, record that discovery.

If you discover that a bizarre historical mechanism exists because of a real ABI or hardware constraint, record that too.

---

# 20. Use experiments to answer architectural questions

When uncertain, construct small experiments.

Examples:

* Can Clang produce binaries accepted by the Symbian loader?
* Which ARM ABI properties actually matter?
* Which Belle libraries are genuinely required for a minimal application?
* What does a SIS package actually contain?
* Which parts of the historical signing pipeline are necessary?
* Which EKA2L1 behaviours differ from the 808?
* Which system APIs are actually required by a Qt application?
* Can a system server be replaced without reproducing unrelated SDK infrastructure?
* Can an old utility's functionality be independently reproduced?
* Which hardware interfaces are accessible from an alternative environment?

Prefer a small experiment over speculative architecture.

---

# 21. Establish milestones

Work incrementally.

## Milestone 0 — Device preservation

Current evidence: archive inventory/hash tools and supplied material are tested;
physical identity, installed firmware, independent offline copy and recovery
appliance remain pending. This is not complete.

Produce:

* exact device identification
* known-good firmware inventory
* immutable firmware/archive
* recovery documentation
* recovery VM
* hardware safety model

Do not experiment with firmware yet.

## Milestone 1 — Native macOS toolchain

Current evidence: the limited LLVM/E32 toolchain is reproducible and independently
validated; ROMless probes and one guarded SDK GUI run pass. General ABI, runtime
and physical compatibility remain acceptance gates.

Produce:

* initial ARM/Symbian toolchain investigation
* minimal compiler/build path
* modern build wrapper
* first reproducible binary

## Milestone 2 — Modern project model

Current evidence: CMake/Ninja, compilation databases, native single-EXE packaging
and standalone ARM GUI IDE setup exist. General project/resource/package profiles
and the actual CLion UI experience remain work.

Produce:

* CMake or equivalent project integration
* compilation database
* clangd support
* modern diagnostics
* package generation

## Milestone 3 — Emulator

Current evidence: native arm64 frontend, preserved ROM/Z import, copied instances
and real GUI pixels/input/zero exit pass on both backends. Native control exposes
status/capture/pointer only; general lifecycle/reset/snapshot orchestration and
full OS boot remain incomplete.

Produce:

* EKA2L1 setup
* Belle environment
* automated installation
* launch
* reset
* snapshots
* screenshots
* logs

## Milestone 4 — Automated application development

Current evidence: build/package/verification and opt-in live GUI Pytest loops
exist. `symbian test` and the general unattended install/run/artifact loop are
not yet implemented; make these the next developer-facing integration slice.

Produce:

```bash
symbian build
symbian test
```

where tests actually execute applications in the emulator.

## Milestone 5 — Modern debugging

Current evidence: relocated live ARM GDB source/ROM stops, stable single stepping
and native process-exit records pass. Full stack traces, panic symbolication,
thread/module inspection and CLion remote-debug validation remain open.

Produce:

* symbols
* crashes
* stack traces
* process/thread inspection
* useful agent-readable diagnostics

## Milestone 6 — Physical-device application deployment

Allow controlled installation of ordinary applications onto the development 808.

The emulator remains the default.

## Milestone 7 — System development

Support:

* servers
* DLLs
* plugins
* IPC
* system components
* lower-level APIs

Primarily tested in EKA2L1.

## Milestone 8 — Hardware research

Investigate lower-level hardware interfaces and firmware only after the preceding infrastructure is reliable.

## Milestone 9 — Alternative operating system

Treat this as a separate project built on the accumulated hardware knowledge.

---

# 22. Alternative OS work

Eventually it may be desirable to run another OS on the Nokia 808.

Do not begin by attempting to reproduce Symbian.

A first meaningful milestone might be:

```text
boot
  |
  v
minimal ARM environment
  |
  v
memory management
  |
  v
scheduler
  |
  v
display/framebuffer
  |
  v
input
```

The first goal could literally be:

> Boot the Nokia 808 and display a diagnostic message without modifying or depending on the existing Belle filesystem.

Investigate the boot chain, hardware and firmware scientifically.

Keep alternative-OS work isolated from the ordinary application-development pipeline.

---

# 23. Quality bar

Do not declare success merely because an old application launches.

The resulting platform should aspire to modern engineering properties:

### Reproducibility

The same source and toolchain should produce reproducible artifacts wherever practical.

### Isolation

Emulator experiments must be disposable.

### Observability

Failures should produce useful machine-readable diagnostics.

### Automation

Build/test/deploy/debug should be scriptable.

### Source intelligence

Modern code navigation and static analysis should work.

### Documentation

Important reverse-engineering discoveries should be recorded.

### Minimalism

Do not retain components merely because they were historically present.

### Safety

The physical phone must have strong operational boundaries.

### Maintainability

Prefer understandable modern code over opaque compatibility machinery.

---

# 24. How to make decisions

When choosing between:

### A — Preserve historical implementation

### B — Wrap historical implementation

### C — Reimplement the underlying functionality

### D — Eliminate the functionality entirely

Do not automatically choose A.

Instead investigate the actual requirement and choose the simplest option that preserves the required behaviour.

A useful decision order is:

```text
Is the functionality actually needed?
        |
       no ──────> eliminate it
        |
       yes
        |
Can modern native code implement it cleanly?
        |
       yes ─────> reimplement
        |
       no
        |
Can a small compatibility layer expose the necessary contract?
        |
       yes ─────> compatibility layer
        |
       no
        |
Is the historical implementation genuinely required?
        |
       yes ─────> preserve/use it
```

Even when preserving something, isolate it behind a clean modern interface.

---

# 25. Do not prematurely build everything

This is a very large project.

At every stage:

1. Establish the smallest useful capability.
2. Test it against real evidence.
3. Document what was learned.
4. Remove unnecessary dependencies.
5. Only then expand the scope.

Avoid building speculative infrastructure for capabilities that have not yet been demonstrated to be necessary.

---

# 26. Working procedure after the initial survey

The initial survey is recorded in docs/RESEARCH.md and subsequent experiments in
docs/RESEARCH_LOG.md. Resume from the implementation review and current STATUS,
inspect the workspace for unfinished changes/processes, then pursue the next
unmet evidence gate. Do not repeat completed archaeology without a new question.

Keep the research document current as scope expands, covering:

1. What is already available.
2. What historical Symbian/Belle components appear genuinely necessary.
3. Which components can plausibly be replaced with modern native implementations.
4. Which historical components may require compatibility treatment.
5. What EKA2L1 already provides.
6. What must be added around EKA2L1.
7. What is required for a native macOS toolchain.
8. What hardware preservation/recovery information is needed.
9. Which operations must remain outside agent authority.
10. A proposed sequence of small, testable milestones.

Do not immediately start implementing a giant compatibility layer.

First understand the system.

When you do begin implementation, prefer small vertical slices that prove important assumptions.

The guiding question throughout the project is:

> **What is the minimum modern machinery required to make the Nokia 808 a genuinely pleasant, deeply observable, programmable computer in 2026?**

Build that—not a museum replica of Nokia's development environment.

