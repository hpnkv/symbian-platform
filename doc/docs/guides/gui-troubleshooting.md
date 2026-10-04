# Troubleshoot the GUI workflow

| Symptom | Check |
| --- | --- |
| Header SHA mismatch or missing file | Exact source pins, local edits, sparse checkout and literal-case paths |
| Duplicate SDK overload declarations | `_UNICODE` and the genuine target compilation command |
| CMake requests GUI SDK or import proxies | Run preparation; check both include cache and manifest proxy paths |
| Unresolved `__aeabi_*` or C++ runtime symbol | Newly added code may require compiler-rt, allocation or a library not yet ported |
| Converter rejects data, TLS or constructors | Current transport deliberately lacks those runtime contracts; inspect the real ELF |
| `.exe` unchanged after an incremental CMake build | Rerun `symbian build` to convert and publish it |
| Empty device list or failure before boot | Matching ROM/Z is missing or the selected instance/storage root is wrong |
| Executable not found | Correct mounted C path, exact virtual path and installed-device selection |
| DLL/ordinal rejection | Compare actual matched EUSER/WS32 exports with manifest proxies; do not fabricate system implementations |
| Panic before drawing | Heap/thread-create/process startup or SDK ABI mismatch; retain log and mapping |
| No redraw or pointer response | Window Server event/client handles, focus, pending request status and target service compatibility |
| No application-menu icon | Check the project-relative SVG path, package file list and AppArc icon path; physical Belle rendering still needs verification |
| SIS packaging fails | Check experimental UIDs, translated captions, SVG XML and bounded file sizes; DLL payloads/scripts/signatures are unsupported |
| Breakpoint never hits | Stub enabled on supported backend, correct current code slide, ARM/Thumb state and exact ELF/executable pair |
| Source files not found in debugger | Apply prefix substitutions rather than removing reproducibility maps |

Captured pixels, pointer-driven redraws, reset and normal SDK exit now have
concrete evidence in the guarded firmware experiment, alongside live debugging.
Next broaden firmware ABI coverage, rotation/focus handling, resource accounting
and application resources/registration.
Imported-app single-EXE SIS installation is already tested separately. A broad platform runtime still needs writable data/BSS/TLS,
static lifetime, compiler-rt/C-library support and a carefully configured C++
library. The evidence trail belongs in [.dev/research-log.md](https://github.com/hpnkv/symbian-platform/blob/main/.dev/research-log.md)
and [.dev/status.md](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md); preserve failures as well as passes.

At this checkpoint the generated E32 SHA-256 is
`2ef4145fa9323837d3d16d8914652d69f0b703a747b4dbb52ab83db83b727792`
and the debug ELF SHA-256 is
`7b2918ba000c8faf039069a3571816a6203edde129a5cb9c2144e475d582d05f`.
Local evidence is under `.symbian/gui-sdk`, `.symbian/gui-app`,
`.symbian/gui-validation`, `.symbian/gui-research`, and
`.symbian/belle-abi-research`. Those directories and
upstream material remain outside version control.

# Installed IDE Run and Debug workflow

The prepared GUI project now has GUI Run and GUI Debug configurations. Open
examples/gui_app in its own IDE window and select clion-arm. GUI Run publishes
the current E32, copies the golden and launches the emulator. GUI Debug uses
CLion Remote Debug plus the separately selected native Symbian GUI GDB profile;
its supervisor publishes/starts a halted copy and automatically relocates
symbols after connection. Set source breakpoints before Resume. The GUI example
must run in the emulator; the CMake ELF is an ARM build product.

[CLion Run and Debug guide](clion-run-debug.md) gives the installed
settings, regeneration command, retained evidence, Stop behavior and toolchain
caching caveat. Inputs/binaries, logs and fresh instances live under
.symbian/gui-runs; source inputs remain unchanged. This is the bounded GUI
experiment, not a complete emulator instance manager or physical-device launch
path. Live launch and GDB/MI checks pass; IDE toolbar operation and complete
frontend stack unwinding are not claimed from those checks.
