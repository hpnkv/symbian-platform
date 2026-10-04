# Inspect symbols and debug the guest

For CLion, the repository root now includes `gui_app` with its actual ARM
compilation command alongside host tooling. Reload CMake, choose `gui_app` for
editing/building and `gui_app_run` for Run. The latter is a native launcher
executable: Run starts the owned Python/EKA2L1 supervisor. Building that target
only compiles the launcher. `gui_app_e32` independently publishes the matching
ELF/E32 pair; the Run supervisor also publishes before launch.

The standalone `examples/gui_app` project and `symbian-pic`/local `clion-arm`
profiles remain usable. Root **GUI Debug** requires the separate **Symbian GUI
GDB** debugger profile; restore the host profile for native tests. Toolchain,
SDK provenance and remote-debug details are in [CLion guide](clion.md).

For clangd, the root `build/debug/compile_commands.json` now contains both host
and guest commands. The published standalone database at `.symbian/gui-app`
also contains the correct ARM macros/SDK includes. Use the database matching the
project being developed.

Check parsing/navigation inputs without booting an emulator:

```sh
"$(brew --prefix llvm)/bin/clangd" \
  --check=examples/gui_app/app.cc \
  --compile-commands-dir=.symbian/gui-app \
  --tweaks=ExpandAutoType
"$(brew --prefix llvm)/bin/llvm-dwarfdump" --verify \
  .symbian/gui-app/gui_app.elf
"$(brew --prefix llvm)/bin/llvm-dwarfdump" --debug-line \
  .symbian/gui-app/gui_app.elf
```

The pinned clangd parses and indexes this source with zero errors using that
limited tweak selection. An unrestricted `--check` also attempts every
refactoring at every token and reports two `ExtractFunction` failures at
loop-control statements; those are refactoring-check failures, not C++
diagnostics. Editor parsing does not require enabling that experimental check.

Debug prefix maps make the two independently built ELFs identical:

| Recorded source prefix | Local replacement |
| --- | --- |
| `/symbian-src/gui_app` | Absolute `examples/gui_app` directory |
| `/symbian-sdk/include` | Absolute `.symbian/gui-sdk/include` directory |
| `/symbian-build/gui_app` | Absolute `.symbian/gui-app/cmake` directory |

DWARF 4 keeps compatibility with older ARM debuggers. Its language tag can read
as C++14 even though actual compiler commands use C++20. The retained `.elf`
contains symbols and line tables; the converted `.exe` is the runtime format,
not the debugger symbol file. Keep each ELF beside the exact executable and
report it produced. `-O1` can optimize locals away and inline model methods;
`DrawGui` is explicitly noinline to give a useful drawing breakpoint.

Offline LLDB inspection has been tested:

```sh
lldb .symbian/gui-app/gui_app.elf
```

In LLDB, replace the `/absolute/repo` prefixes with this checkout's actual path:

```text
settings set target.source-map /symbian-src/gui_app /absolute/repo/examples/gui_app /symbian-sdk/include /absolute/repo/.symbian/gui-sdk/include
image lookup -n GuiMain
image lookup -r -n DrawGui
source list -n GuiMain
quit
```

LLDB identifies the object as ARM and shows its source. This does not attach to
the guest. Attaching LLDB to the macOS EKA2L1 process instead debugs the ARM64
host emulator, which is useful for emulator failures but a different target.

# 9. Debug guest startup and the GUI

**Guest attachment, startup source breakpoints and instruction stepping have
been verified against the supplied RM-807 image.** Heap initialization fails
before drawing; visual GUI behavior and a normal SDK exit remain unverified.
Debugging startup is useful even while those runtime contracts are incomplete.
The pinned emulator's
[GDB documentation](https://github.com/EKA2L1/EKA2L1/blob/2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8/src/emu/scripting/lua/eka2l1/topics/DebuggingWithGDB.md)
describes whole-guest debugging, Dynarmic and software breakpoints. Its stub is
not a per-process Symbian debug agent. Start with that supported documented CPU
path and a bootable device; no stub connection is available from ROMless help.

Homebrew has an
[ARM GDB formula](https://formulae.brew.sh/formula/arm-none-eabi-gdb).
Installation is a debugger prerequisite, not evidence of stub compatibility:

```sh
brew install arm-none-eabi-gdb
arm-none-eabi-gdb --version
```

First launch once with GDB disabled. Search the instance log for this image:

```sh
rg -i 'gui_app|runtime code|ordinal|panic' "$SYMBIAN_GUI_INSTANCE/EKA2L1.log"
```

The pinned kernel reports `gui_app ... runtime code: 0x...` on attachment.
Record the **actual** code address. The ELF/E32 link code base is `0x8000`.
The symbol slide is `actual_runtime_code_base - 0x8000`; do not assume a
particular guest address or reuse it after a different launch without checking.

Quit the emulator. Edit the existing instance `config.yml`, preserving its
device/storage settings and changing these keys:

```yaml
cpu: dynarmic
enable-gdb-stub: true
gdb-port: 24689
```

Useful optional trace keys in this pinned source are `log-svc`, `log-ipc`,
`log-read`, `log-write`, and `log-exports`. Start with the specific trace needed;
full service logging can be noisy and slow. Logs and settings are instance-local
only with the applied root patch. Restart using the same `--run` command.
The applied patch binds the GDB port to IPv4 loopback. Confirm its startup log;
an occupied port or an unbootable selected device must be fixed first.

Start `arm-none-eabi-gdb` in the repository root. In the following commands
replace `SLIDE` with the calculated hexadecimal slide and `/absolute/repo`
with the actual checkout path **before entering them**:

```text
set pagination off
set architecture arm
set remotetimeout 200
file /absolute/repo/.symbian/gui-app/gui_app.elf
symbol-file -o SLIDE /absolute/repo/.symbian/gui-app/gui_app.elf
set substitute-path /symbian-src/gui_app /absolute/repo/examples/gui_app
set substitute-path /symbian-sdk/include /absolute/repo/.symbian/gui-sdk/include
target remote 127.0.0.1:24689
break GuiRunThread
continue
```

[GDB's `symbol-file -o` contract](https://sourceware.org/gdb/current/onlinedocs/gdb.html/Files.html)
adds the offset to section addresses. Verify the loaded symbols against the
new launch's mapping. A preliminary launch helps discover the address but does
not guarantee identical mapping next time. If it differs, interrupt and reload
symbols with the corrected slide and recreate breakpoints.

Once stopped in the correct image, use `info registers`, `bt`, `list`,
`step`, `next`, and `x/8i $pc`. Add a source-line breakpoint inside `DrawGui`
using its current line number from `window_server.cc`; inspect model/layout values where
optimization leaves them available. Startup assembly executes ARM instructions
and C++ executes Thumb. For raw-address breakpoints, upstream documents
`set arm fallback-mode thumb` for Thumb addresses; use ARM for startup instead.
For raw disassembly check CPSR bit `0x20`: use `set arm force-mode arm`
when clear and `set arm force-mode thumb` when set, returning to `auto`
for symbolized source. An incorrect fallback can misdecode ARM as Thumb.
Do not treat the Thumb state bit as a separate byte of code.

With the default executive profile, the launch stops at `GuiRunThread`,
PC `0x700009da`, with reason zero
and thread-create information at `0x40ffc0`. Two `stepi` commands stop at
`0x700009dc` and `0x700009de`. A fresh register read after a delay confirms the
first stop stays halted. Source path substitution displays the actual adapter.
A breakpoint at `startup.cc:19` observes heap result `-1` (`KErrNotFound`),
so `GuiMain` is never called in this experiment.

Live ROM breakpoints identify the failing executive-call boundary:

| Guest instruction address | Belle call | Pinned emulator behavior |
| --- | --- | --- |
| `0x804bf730` | SVC `0x51`, kernel HAL page-size query; arguments `0,7,&size,0` | Unimplemented; its epoc10 table uses `0x4F` for HAL |
| `0x804bf810` | SVC `0x6D`, chunk creation; owner `1`, `$HEAP` descriptor and chunk-create structure | Dispatches object lookup; chunk creation is registered at `0x6B` |
| `0x804bfc40` | SVC `0xF7`, called from the real `User::Exit(-1)` path | Unimplemented; its thread-exiting handler is registered at `0xF6` |

The HAL/chunk identifications combine real guest argument/instruction reads
with the original SDK implementations. The exit identification is consistent
with its caller and the original exit code. These discrepancies establish a
kernel executive ABI problem for this firmware; they do not establish a complete
Belle call table. Do not shift the whole table or substitute SDK implementations
based on these three calls. Subsequent source/export wrapper analysis supports
a piecewise experimental profile and retains default-profile controls. See
[Belle ABI research](https://github.com/hpnkv/symbian-platform/blob/main/.dev/belle-abi.md) for its derivation and remaining gaps.
To select it for a disposable launch, prefix the emulator command with
`EKA2L1_EXPERIMENTAL_SVC_PROFILE=rm807-113.010.1508`; it requires the exact
preserved ROM digest. The initial drawing function now completes with SDK
cleanup-stack setup and a separate ARM TPIDRURO register. This does not yet
establish displayed pixels, input delivery, complete DLL initialization or exit.

Replay the bounded live debugging regression with the exact preserved fixture:

```sh
SYMBIAN_GUI_DEBUG_GOLDEN_ROOT="$PWD/.symbian/instances/delight-import-01" \
SYMBIAN_GUI_DEBUG_BUILD="$PWD/.symbian/gui-app" \
SYMBIAN_EKA2L1_EXECUTABLE="$SYMBIAN_EMULATOR" \
uv run pytest -q symbian/tests/test_guest_debugger.py \
  --basetemp .symbian/gui-debug-check-01
```

Use a new disposable `--basetemp` directory: Pytest clears that directory.
The test checks ROM/EUSER and ELF/E32 digests before starting, copies the golden
state, uses an instance-local loopback port, checks real source and ROM stops,
retains logs, and verifies those input digests again. It explicitly expects the
default heap failure. The explicit guarded-profile case reaches and returns from
the initial `DrawGui` function, inspecting the zero counter and 360 by 640
layout. Two further cases check actual rejection of an unknown profile and
a one-byte-modified private ROM. No input means a visible skip. Its cleanup stops only its
own frontend process; the current frontend may require KILL after TERM. Neither
that forced stop nor reaching `User::Exit` proves normal guest cleanup.

The separate autonomous GUI test now verifies rendered redraws, pointer delivery
and normal exit on both backends. Enable the private native control socket and
replay its pixel/exit checks using [EMULATOR_CONTROL.md](emulator-control.md).
Keep those evidence flags separate from attachment and stepping. Source
symbols in a static LLDB session are not a substitute for the live test.
Guest stack unwinding, crash symbolication,
LLDB remote compatibility and full process inspection remain separate work.

