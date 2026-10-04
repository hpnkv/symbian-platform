# Run the GUI in a disposable emulator

This step runs the ARM E32 image in EKA2L1, then checks visible pixels and
pointer input. The captures below come from a guarded test against the named
RM-807 research firmware fixture. They show the counter before and after one
tap; they are emulator evidence, not a Nokia 808 screen capture.

| Initial frame | After one increment |
| --- | --- |
| ![Four-digit counter showing 0000.](../assets/screenshots/gui-counter-initial.png){ width="250" } | ![Four-digit counter showing 0001.](../assets/screenshots/gui-counter-one.png){ width="250" } |

Use pinned EKA2L1 commit `2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8` and
the ordered local patches. `instance-root.patch` supplies isolated macOS data/settings roots,
bounded CLI failure shutdown, and a loopback-only GDB listener.
`runtime-probe.patch` supplies the documented research initialization fixes.
`guest-debug-step.patch` makes a remote single step stop after one instruction
and send its stop response. Without it, stepping continues silently through
the guest until another breakpoint, so register observations can be misleading.
`guest-debug-library-query.patch` removes an unsupported GDB capability.
`symbian101-experimental.patch` supplies explicitly selected, ROM-digest-guarded
executive routing; `guest-thread-register.patch` adds separate TPIDRURO state
and tested macOS backend context preservation. `guest-control.patch` links the
GPL native adapter for screen capture, logical pointer input and final exit
records; it is disabled unless an explicit private socket is supplied. These
are already applied in
the current workspace. For a fresh checkout:

```sh
brew install qtbase qttools qtsvg
git clone --no-checkout https://github.com/EKA2L1/EKA2L1 \
  research/upstream/EKA2L1
git -C research/upstream/EKA2L1 checkout \
  2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8
git -C research/upstream/EKA2L1 submodule update --init --recursive --depth 1
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/instance-root.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/runtime-probe.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-debug-step.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-debug-library-query.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/symbian101-experimental.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-thread-register.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/guest-control.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/firmware-import-bounds.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/fbs-unsupported-request.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/background-window.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/dll-wsd-dyncom-exit.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/belle-library-entry-start.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/belle-library-load-prepare.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/belle-thread-exit-reason.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/dyncom-strexd-value.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/v10-thread-exit-reason.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/ntick-fast-counter-hal.patch
git -C research/upstream/EKA2L1 apply ../../../research/eka2l1/fast-counter-rate.patch
git clone --no-checkout \
  https://github.com/SymbianSource/oss.FCL.sf.os.buildtools \
  research/upstream/buildtools
git -C research/upstream/buildtools checkout \
  7b35cd328d3a5e8e0bc177d0169fd409c3273193
(cd research/upstream/EKA2L1/src/external/ffmpeg && sh macos_arm64-build.sh)
```

Skip acquisitions and patches already present; inspect before changing existing
research trees. The ARM64 FFmpeg step is needed because the pinned submodule's
bundled macOS libraries are Intel-only. Preserve EKA2L1's GPL and other licenses.
The injected hook fetches the same pinned Abseil revision as the host platform.
An existing matching checkout can be selected with `SYMBIAN_ABSEIL_SOURCE_DIR`
(or its CMake cache variable). The hook explicitly selects C++20 before
configuring Abseil because it runs before upstream's standard selection.
Configure the Apple Silicon build and separate historical oracle executables:

```sh
cmake -S research/upstream/EKA2L1 -B build/eka2l1 -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)" -DCMAKE_POLICY_VERSION_MINIMUM=3.5 \
  -DEKA2L1_BUILD_TESTS=ON -DEKA2L1_BUILD_TOOLS=OFF \
  -DEKA2L1_ENABLE_QT_CAMERA=OFF -DEKA2L1_SCRIPTING_LUA=OFF \
  -DCMAKE_PROJECT_EKA2L1_INCLUDE="$PWD/research/eka2l1/project-tests.cmake"
cmake --build build/eka2l1 -j 8 --target \
  eka2l1_qt symbian_checksum_oracle symbian_validator_oracle \
  symbian_sis_checksum_oracle symbian_gui_package_probe symbian_control_probe
```

`kernelhwsrv` is the same pinned tree already used for headers. The historical
checksum and validator remain separate oracle binaries; their algorithms are
not linked into the platform core. This is a local development app bundle,
not a signed redistributable application. The verified Qt was 6.11.2; the
research README records deployment-target/library warnings and the broader
process/installer test targets.

Select a disposable instance root, even for help:

```sh
export SYMBIAN_EMULATOR="$PWD/build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
```

The two task-specific variables must be absolute paths. Help works without a
ROM. The patched frontend's working directory and `config.yml`, resources,
logs and default Qt settings are beneath the instance root. Default
`data-storage: data` then places guest storage in `$SYMBIAN_GUI_INSTANCE/data`.
If you change `data-storage`, subsequent drive paths must use that actual
setting. The Cocoa Qt plugin is bundled; a headless offscreen GUI runner has
not been established.

# 7. Boot a real test device and launch the GUI

The basic asset import and direct-launch attempt have now been executed with
the supplied firmware; the visual/input acceptance tests below remain open.
EKA2L1's
[installation instructions](https://github.com/EKA2L1/EKA2L1/wiki/Using-the-emulator)
require a ROM and a repackaged Z drive from the same device. Public headers and
ordinal proxies cannot replace those assets. Use legitimate, independently
preserved material and record its digests and actual device identity. A generic
epoc10 test profile does not identify a Belle firmware. RM-807 is still a
hypothesis for the physical phone in this project. The supplied ZIP declares
RM-807; that establishes the archive's identity, not the phone's current state.

## Supplied Delight v1.8 archive checkpoint

The provided Downloads ZIP has SHA-256
`88403c3a8ef5ed14a48712a70fd81020b2b33ae17d5487a595004f09b8c10d98`.
It contains seven files: core/ROFS2/ROFS3/UDA FPSX images, a VPL, a DCP and a
signature file. Its VPL declares product `059M7Q4`, version `113.010.1508`, and
a French/Euro variant. All required files are present; ZIP CRCs and all supplied
VPL CRC entries agree. Some optional files, including the eMMC image, are absent.
CRC agreement and a signature file do not establish authenticity. The filename
identifies custom Delight material; it is not an established factory recovery
baseline or a verified copy of this physical phone.

A working extraction is in `.symbian/assets/delight-v1.8/RM-807`. The separate
native research importer calls EKA2L1's actual VPL/FPSX/ROM/ROFS/FAT pipeline.
It refuses an existing output root and never calls a physical transport:

```sh
cmake --build build/eka2l1 --target symbian_firmware_import_probe -j 8
SYMBIAN_FIRMWARE_VPL="$PWD/.symbian/assets/delight-v1.8/RM-807/RM807_059M7Q4_113.010.1508_019.vpl" \
  SYMBIAN_EMULATOR_IMPORT_ROOT="$PWD/.symbian/instances/delight-import-new" \
  build/eka2l1/platform-tests/symbian_firmware_import_probe \
  --gtest_output=json:"$PWD/.symbian/delight-import-new.json"
```

The retained successful root is `.symbian/instances/delight-import-01`, with
13,438 inventoried files, a 31 MiB `data/roms/rm-807/SYM.ROM`, actual EUSER/WS32
in `data/drives/z/rm-807/sys/bin`, and isolated writable drives. Its
`data/devices.yml` identifies Nokia/808 PureView/RM-807/epoc100; machine UID was
initially recorded as zero and must not be inferred from this import alone.
Copy the complete stopped imported root before launch, wait for copying to
finish, and use the new copy as `SYMBIAN_GUI_INSTANCE`. The direct-launch attempt
in `delight-gui-01` mapped the GUI at `0x70000000` and both real system DLLs;
it logged unimplemented SVCs `0x51`/`0xF7` and a `$HEAP` lookup failure.
That is runtime diagnostic evidence, not proof of the counter rendering or
working. Guest symbols, heap startup and emulator service coverage need the
next debugging experiment. Original archive/extracted/imported digests and
logs stay in ignored `.symbian/gui-research`.

Launch the patched frontend:

```sh
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR"
```

That direct `.app` executable launch follows macOS's normal foreground-app
behavior. SDK `Run`/`Debug` and guarded GUI tests instead launch a private
symlink outside the bundle and set the maintained background-window profile.
They keep the current terminal or IDE active. The OpenGL display is a normal
managed desktop-Space window that cannot join or tile in another app's
full-screen Space; clicking it later may intentionally focus it. The policy
is built and a real desktop GUI run stayed behind the active app. Placement
while another app is already full-screen still needs a visual check.

Use **File/Install device** to select the preserved ROM and corresponding Z
package. Choose separate storage for this device if offered. Verify that the
installed device boots and the display accepts normal input before attempting
the new executable. Record firmware code, Symbian version and screen dimensions.
Select that device in the frontend. The CLI's `--device` option expects the
emulator's recorded firmware code; do not invent one from the phone model.

Quit the emulator before changing its virtual filesystem. With default storage,
the host C-drive mapping is one of:

| Device storage setting | Host C directory |
| --- | --- |
| Shared drives | `$SYMBIAN_GUI_INSTANCE/data/drives/c` |
| Separate drives | `$SYMBIAN_GUI_INSTANCE/data/drives/<lowercase-firmware-code>/c` |

These paths come from the pinned `device_drive_folder` implementation. Confirm
the selected device and mapping; do not create both paths and guess which is
mounted. For a shared-drive instance, copy only the generated executable:

```sh
export SYMBIAN_GUI_C_DRIVE="$SYMBIAN_GUI_INSTANCE/data/drives/c"
mkdir -p "$SYMBIAN_GUI_C_DRIVE/sys/bin"
cp .symbian/gui-app/gui_app.exe "$SYMBIAN_GUI_C_DRIVE/sys/bin/gui_app.exe"
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" \
  --run 'C:\sys\bin\gui_app.exe'
```

For separate drives, set `SYMBIAN_GUI_C_DRIVE` to the confirmed per-device C
directory first. All paths in this step refer to disposable emulator storage.
The equivalent command `--app` is an alias of `--run` in this pinned frontend.
The absolute virtual EXE path is necessary because this example has no
application registration. Keep `gui_app.elf` on the host for debugging.

Alternatively, after confirming the selected disposable device and drive,
install `.symbian/gui-package/gui_app.sis` through the frontend's package
installer. The pinned CLI `--install` uses drive E, so a CLI installation must
subsequently launch `E:\sys\bin\gui_app.exe`, not the C-drive path above.
Ordinary installation still does not create an application-menu entry without
registration resources. The native research package tests explicitly select C.

Test and record the following in `.dev/research-log.md` with actual results:

1. On launch, the screen shows `0000` and all three controls, with no panic.
2. Left-control taps show `0001`, `0002`, and so on. The middle control resets to `0000`.
3. Taps in gaps and outside controls leave the count unchanged; a single
   button-down increments once. Saturation at 9999 is already model-tested;
   test it visually if automated pointer injection becomes available.
4. The right control exits. Launch again and verify the count starts at zero; record whether
   session/window resources and address space are released normally.
5. Obscure and restore the window and verify redraw. Launch in each intended
   fixed orientation; rotating while running is not supported yet.
6. With the emulator stopped, test separate copies with `cpu: dynarmic` and
   `cpu: dyncom` in `config.yml`, GDB disabled. Record differences and failures.

Preserve the matching build/validation reports, input asset digests, selected
device, actual code-load address, screenshot and instance `EKA2L1.log`.
`EKA2L1_TakeThis.log` holds a previous log when the frontend rotates it.
If imports or startup fail, preserve the failure: it identifies a real missing
runtime/ABI contract. Do not replace system libraries with link proxies.

For a baseline, quit the emulator and copy its complete stopped instance into
a separate golden directory; make a fresh copy for each experiment. This is
filesystem restoration, not a full-machine snapshot, and the snapshot workflow
has not been validated for every subsystem. Preserve firmware originals in an
independently held offline copy with recorded digests; read-only permissions
alone do not make the owner unable to change them. No physical-device recovery,
flashing, calibration, partition or bootloader operation is part of this recipe.
