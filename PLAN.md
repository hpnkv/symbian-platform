# Mission: Revive the Nokia 808 / Symbian Belle Development Platform for 2026

You are working on a long-running engineering and research project whose goal is to create a **modern, macOS-native development stack for the Nokia 808 PureView and the Symbian Belle ecosystem**, suitable for serious application development, system-component development, reverse engineering, debugging, experimentation, and potentially eventually developing an alternative operating system for the hardware.

The goal is **not** to recreate Nokia's historical development environment verbatim.

The goal is to understand what the historical platform actually did, determine what is genuinely required to produce and execute software on the hardware, and then build the smallest, cleanest, most maintainable modern system that provides those capabilities.

Think of this project as:

> **"What would the Symbian/Nokia 808 development platform look like if it had survived and been redesigned for 2026?"**

rather than:

> "How do we reproduce Nokia's 2010 development environment?"

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

Before experimental development:

1. Identify the exact device variant.
2. Record its current software/firmware state.
3. Preserve all available firmware and ROM material.
4. Preserve relevant filesystem/device data where possible.
5. Hash preserved artifacts.
6. Document recovery procedures.
7. Establish a known-good baseline.

Create an immutable reference archive.

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

Produce:

* exact device identification
* known-good firmware inventory
* immutable firmware/archive
* recovery documentation
* recovery VM
* hardware safety model

Do not experiment with firmware yet.

## Milestone 1 — Native macOS toolchain

Produce:

* initial ARM/Symbian toolchain investigation
* minimal compiler/build path
* modern build wrapper
* first reproducible binary

## Milestone 2 — Modern project model

Produce:

* CMake or equivalent project integration
* compilation database
* clangd support
* modern diagnostics
* package generation

## Milestone 3 — Emulator

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

Produce:

```bash
symbian build
symbian test
```

where tests actually execute applications in the emulator.

## Milestone 5 — Modern debugging

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

# 26. Initial task

Begin by surveying the current state of the project directory and repository.

Then produce a technical research document containing:

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

