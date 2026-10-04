# Provisional Linux host support

Status: implementation preparation. Host native and wheel evidence exists for
Linux aarch64; application SDK export, GUI emulator, guest GDB, Console visuals
and physical-device flows have not been validated end to end on an interactive
Linux host. The public entry is `doc/docs/guides/linux.md`.

## Scope and dependency policy

Reuse A11's isolated static dependency prefix, CMake/Ninja presets, GTest,
installed-wheel audit and narrow exception boundaries. Keep the same guest ARM
toolchain, E32 converter, firmware resolver and SDK sources; choose host tools
and emulator paths by platform. Do not add a second native scheduler, network
stack, Python format parser or Linux-only guest ABI. Keep original EKA2L1,
Symbian and third-party licenses.

## Gates for a Linux development machine

1. On a clean x86_64 and an aarch64 Linux machine, run the host native presets,
   CTest, `uv sync`, Pytest, wheel build and installed-wheel audit. Record the
   distro, compiler, CMake/Ninja, Python and library versions. CI wheel success
   does not by itself establish an interactive developer workflow.
2. Prepare pinned public upstream source and run `symbian sdk install` from
   source. Audit every installed digest, verify the selected LLVM C/C++/LLD
   tools and the Linux `bin/eka2l1_qt` manifest path, and build an independent
   ARMv5T and ARMv6 application consumer. Record failures in host tool or
   resource compilation rather than treating them as guest ABI failures.
3. Build the patched pinned EKA2L1 frontend on Linux x86_64. The tree has an
   x86_64 FFmpeg script; aarch64 needs its own compatible recipe. Import a
   separately supplied firmware fixture, run a disposable GUI session on
   Dynarmic and Dyncom, inspect pixels/input and guest exit, then test cleanup
   and preserved-baseline digests. Check Wayland and X11 focus/input separately
   where available; macOS background-window results do not transfer.
4. Test `symbian emu configure-ide`, CMake indexing and a real guest GDB
   breakpoint with the available `arm-none-eabi-gdb` or `gdb-multiarch`. Keep
   host debugger and ARM guest debugger profiles distinct. Measure process and
   symbol relocation behavior in the IDE, not only in its saved XML.
5. Open the Console through the Linux PySide6 renderer, navigate and build a
   project, then run a supervised emulator session. Test USB discovery and
   bounded read-only protocols with a connected Linux handset separately.
   Physical Symbian application compatibility remains a separate on-device
   gate under the existing device policy.

For each gate, save exact commands, package versions, artifacts, logs and
negative controls in `.dev/status.md` and questions in `.dev/research-log.md`.
Do not label the full Linux workflow supported until the corresponding host
results exist.
