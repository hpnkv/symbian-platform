# Guest debugging details

EKA2L1's remote GDB stub debugs the ARM guest. A native debugger attached
to EKA2L1 itself debugs the host emulator. For the regular GUI workflow, use
[Debug the GUI guest](../guides/gui-debug.md) and the saved
[CLion Run/Debug profiles](../guides/clion-run-debug.md).

## Symbol files and source mapping

Keep the exact ELF that produced the E32 image. The E32 is the loader input;
DWARF symbols and line tables remain in the ELF. The maintained GUI example
publishes both under `.symbian/gui-app/`.

Its source prefixes are stable for reproducible builds:

| Recorded prefix | Local source |
| --- | --- |
| `/symbian-src/gui_app` | `examples/gui_app` |
| `/symbian-sdk/include` | `.symbian/gui-sdk/include` |
| `/symbian-build/gui_app` | `.symbian/gui-app/cmake` |

Inspect the ELF and compilation database before booting:

=== "macOS"

    ```sh
    "$(brew --prefix llvm)/bin/llvm-dwarfdump" --verify \
      .symbian/gui-app/gui_app.elf
    "$(brew --prefix llvm)/bin/clangd" --check=examples/gui_app/app.cc \
      --compile-commands-dir=.symbian/gui-app
    ```

=== "Linux (provisional)"

    ```sh
    llvm-dwarfdump --verify .symbian/gui-app/gui_app.elf
    clangd --check=examples/gui_app/app.cc \
      --compile-commands-dir=.symbian/gui-app
    ```

    Install `clangd` and `llvm-dwarfdump` from the same LLVM toolchain used
    by the build if your distribution splits them into separate packages.

Compiler parsing and DWARF inspection do not prove guest execution. Optimized
locals may be unavailable at a breakpoint.

## Manual GDB session

The guest's actual load address comes from its run log. The GUI ELF link code
base is `0x8000`; calculate the symbol slide as actual runtime code base minus
`0x8000`. Do not reuse a slide from another session. The pinned research
frontend binds the GDB stub to loopback when enabled in the disposable
instance's `config.yml`:

```yaml
cpu: dynarmic
enable-gdb-stub: true
gdb-port: 24689
```

After launching the image with the stub enabled, open `arm-none-eabi-gdb`
(or `gdb-multiarch` on Linux) and
replace the paths and `SLIDE` with values from this run:

```text
set architecture arm
file /absolute/repo/.symbian/gui-app/gui_app.elf
symbol-file -o SLIDE /absolute/repo/.symbian/gui-app/gui_app.elf
set substitute-path /symbian-src/gui_app /absolute/repo/examples/gui_app
target remote 127.0.0.1:24689
break GuiRunThread
continue
```

The stub controls the whole guest, rather than providing a per-process Symbian
debug agent. Retain the selected firmware, E32/ELF hashes, run log, load
address and guest exit result with observations. The [emulator control
guide](../guides/emulator-control.md) covers managed sessions.
