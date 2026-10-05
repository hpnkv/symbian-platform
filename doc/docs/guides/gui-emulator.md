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

## Open the emulator for manual application launch

Run the frontend using the selected firmware without automatically starting an
application:

```sh
uv run symbian emu run
uv run symbian emu run --firmware my-phone --backend dynarmic
```

In **Symbian Console → Emulator**, choose **Run emulator**. Optional settings
select the firmware, SDK, project configuration or CPU backend. A project is
used to resolve settings and, when it declares an application, to build and
stage that application. The CLI detects an application in its working directory;
`--project /path/to/app` selects one explicitly. The GUI uses its selected
application context. Registered applications receive their SDK-compiled menu
resources in the private emulator C drive so they appear in EKA2L1's application list. Select the
application there when ready to run it. Plain executable projects are copied
to `C:\sys\bin`; they have no application-menu registration.

Without an application context, the emulator opens without a build or injection.
EKA1 firmware can open in that mode; staging an SDK application still requires
the supported EKA2 application ABI.

Each launch copies the verified baseline into `.symbian/emulator-runs/` under
the chosen workspace. Close the EKA2L1 window to finish, or press Ctrl+C in the
CLI to stop its owned child. The session retains `frontend.log`, `launch.json`
and any final control report. Changes to the copied instance do not carry into
the next launch. Opening the frontend does not establish full OS boot or
physical-device compatibility.

## Built-in firmware application compatibility

An application appearing in EKA2L1's list establishes registration discovery;
each application still depends on the emulator's kernel and service support.
The SDK counter exercises a small Window Server path. Firmware applications
can require additional AVKON, media, database and file-system services.

Fresh-instance checks against the preserved RM-807 113.010.1508 fixture with
the experimental Belle profile found:

| Application | Observed result |
| --- | --- |
| SDK counter | Renders |
| Calculator | Renders and accepts a digit tap |
| Settings | Renders its initial list; subviews untested |
| Gallery, both registered entries | Black screen; media Harvester exits with `KErrGeneral` (`-2`) |
| Clock | Black screen; cause not yet isolated |

Gallery reproduces on Dynarmic and Dyncom, with and without an injected SDK
application. Waiting 30 seconds on Dynarmic does not produce a display.
Its logs expose unsupported file-system plugin loading/mounting/opening and
missing executive calls. These are emulator compatibility gaps; the precise
dependency that prevents Gallery's first frame remains unresolved. A working
Calculator or Settings screen does not establish complete firmware support.

For a failed app, retain the session's `frontend.log`, selected firmware and
profile, app name/UID, capture and native process-exit report. The investigation
and private evidence paths are recorded in the repository's
`.dev/research-log.md`.
