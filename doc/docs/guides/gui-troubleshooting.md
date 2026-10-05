# Troubleshoot the GUI workflow

| Symptom | Check |
| --- | --- |
| Header SHA mismatch or missing file | Exact source pins, local edits, sparse checkout and literal-case paths |
| Duplicate SDK overload declarations | `_UNICODE` and the genuine target compilation command |
| CMake requests GUI SDK or import proxies | Run preparation; check both include cache and manifest proxy paths |
| Unresolved `__aeabi_*` or C++ runtime symbol | Newly added code may require compiler-rt, allocation or a library not yet ported |
| Converter rejects data, TLS or constructors | Check supported section/relocation forms; general TLS and local-static guards are unsupported |
| `.exe` unchanged after an incremental CMake build | Rerun `symbian build` to convert and publish it |
| Empty device list or failure before boot | Matching ROM/Z is missing or the selected instance/storage root is wrong |
| Executable not found | Correct mounted C path, exact virtual path and installed-device selection |
| DLL/ordinal rejection | Compare actual matched EUSER/WS32 exports with manifest proxies; do not fabricate system implementations |
| Panic before drawing | Heap/thread-create/process startup or SDK ABI mismatch; retain log and mapping |
| No redraw or pointer response | Window Server event/client handles, focus, pending request status and target service compatibility |
| No application-menu icon | Check the project-relative SVG path, package file list and AppArc icon path; physical Belle rendering still needs verification |
| SIS packaging fails | Check experimental UIDs, translated captions, SVG XML and bounded file sizes; DLL payloads and scripts are unsupported; check signing identity and key when signing |
| Breakpoint never hits | Stub enabled on supported backend, correct current code slide, ARM/Thumb state and exact ELF/executable pair |
| Source files not found in debugger | Apply prefix substitutions rather than removing reproducibility maps |

## IDE Run and Debug

The prepared GUI project now has GUI Run and GUI Debug configurations. Open
examples/gui_app in its own IDE window and select clion-arm. GUI Run publishes
the current E32, copies the golden and launches the emulator. GUI Debug uses
CLion Remote Debug plus the separately selected native Symbian GUI GDB profile;
its supervisor publishes/starts a halted copy and automatically relocates
symbols after connection. Set source breakpoints before Resume. The GUI example
must run in the emulator; the CMake ELF is an ARM build product.

[CLion Run and Debug guide](clion-run-debug.md) gives the installed
settings, regeneration command, logs, Stop behavior and toolchain
caching caveat. Inputs/binaries, logs and fresh instances live under
.symbian/gui-runs; source inputs remain unchanged. Debug the guest with ARM GDB; a host debugger attached to the launcher inspects
the launcher or emulator process.
