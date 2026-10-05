# Debug the GUI guest

Keep the built ARM ELF beside its E32 image. The ELF contains source and symbol
information; the emulator loads E32. A guest breakpoint therefore needs the
matching ELF and the guest's actual load address.

## 1. Prepare the example

Build and run [the GUI example](gui-build.md) once with a named firmware
selection. Keep `.symbian/gui-app/gui_app.elf` and the run report. A source
walkthrough can use the saved **GUI Debug** configuration in [CLion](clion-run-debug.md).
It launches a halted emulator and connects ARM GDB through the SDK wrapper.

## 2. Place a breakpoint

Set a breakpoint in `GuiRunThread` for startup or `DrawGui` for drawing. Start
**GUI Debug**, continue from the initial halt, and use the guest input to reach
your breakpoint. Inspect the call stack and local values in the ARM session.
Host LLDB attached to the EKA2L1 process debugs the emulator itself, which is
a different target.

## 3. Diagnose a failed session

Record the actual load address, ELF/E32 pair, firmware identity, guest exit
reason and emulator log with any breakpoint result. The [guest debugging
reference](../reference/guest-debugging.md) gives manual ARM GDB commands,
source mappings and startup troubleshooting.
