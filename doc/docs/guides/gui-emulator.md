# Run the GUI example in the emulator

The `examples/gui_app` counter uses Symbian Window Server to draw four digits
and respond to taps. EKA2L1 is the community emulator used to inspect this
behavior with a locally supplied ROM/Z image.

| Initial frame | After one increment |
| --- | --- |
| ![Counter at 0000.](../assets/screenshots/gui-counter-initial.png){ width="250" } | ![Counter at 0001.](../assets/screenshots/gui-counter-one.png){ width="250" } |

*These are captures from a guarded run against the named RM-807 research
fixture. They are emulator evidence, not screenshots from a physical phone.*

## 1. Prepare the image and firmware

[Build the GUI executable](gui-build.md) and [import a local firmware
image](firmware.md). The example's E32 path is
`.symbian/gui-app/gui_app.exe`; keep the matching `.elf` for debugging.

## 2. Launch a disposable session

The SDK's **GUI Run** configuration in [CLion](clion-run-debug.md) invokes the
run supervisor. It resolves the selected firmware, copies its verified
baseline into fresh writable state, stages the image and starts EKA2L1.
Use the emulator's visible window to tap the increment, reset and exit
controls.

A manual pinned-source build, private fixture preparation and direct
`--run` workflow are in the [research emulator reference](../reference/emulator-source-build.md).
They are needed when investigating the emulator itself.

## 3. Record the result

Check that the counter starts at `0000`, increments once per tap, resets and
exits normally. Keep the selected firmware identity, CPU backend, screenshots,
guest exit reason and instance log. If the GUI fails before drawing, retain
the failure log and use [guest debugging](gui-debug.md). A working emulator
session does not prove Nokia 808 compatibility.
