# Firmware and emulator compatibility

Select a local image using the [firmware guide](../guides/firmware.md).
Firmware is supplied separately; its DLL exports and system services determine
which application profiles can run.

## Application requirements

The modern C++ runtime requires the ARM EABI/EKA2 application ABI and the
imports used by the selected components. The GUI starter needs EUSER, WS32 and
GDI; the task/thread profile also requires `libpthread.dll`. Missing DLLs are
reported before process creation. Building without firmware is possible.
[EKA1 executables](../guides/eka1.md) use a separate legacy profile; they cannot
use the modern GUI or runtime archives.

The starter adapts to displays down to 176x208 logical pixels and supports
touch, Enter/centre, Backspace and Clear/Exit softkeys. The model retains twelve
rows and displays the newest rows that fit. Full orientation handling and all
vendor/server ABIs are unsupported.

## Executable compatibility matrix

The checked-in `scripts/check_firmware_compatibility.py` copies a named
firmware fixture before running an E32 image. Its JSON includes the firmware
identity, image digest, direct DLL/ordinal imports, missing direct DLL names,
selected executive profile, frontend and guest exits, and a frame digest for
interactive apps. E32 inspection establishes conversion; an emulator process
exit and control record establish loader acceptance and guest execution. A
captured frame alone does not establish input handling or visual correctness.
The script always reports `physical_device_tested: false`.

For example, after importing a firmware fixture and building the software
ARMv5T app, run:

```sh
python -m scripts.check_firmware_compatibility \
  --store .symbian/firmware-store --firmware e71 \
  --image build/guest-probes-armv5t/examples/sdl2_app/e32/sdl2_app.exe \
  --emulator /path/to/EKA2L1 --interactive-uid 0xe0000e20
```

| Named fixture | Selected profile | SDL2 | SDL3 | Evidence |
| --- | --- | --- | --- | --- |
| E71/RM-346 | ARMv5T older EKA2, software | Normal guest exit | Normal guest exit | Source-built images; direct imports present and emulator loader accepted them |
| 6120c/RM-243 | ARMv5T older EKA2, software | Normal guest exit | Normal guest exit | Source-built images; direct imports present and emulator loader accepted them |
| C7/RM-675, E6/RM-609 | ARMv6 GPU | Normal guest exit | Normal guest exit | Source-built images; direct imports present and emulator loader accepted them |
| Nokia 808/RM-807 | ARMv6 GPU | Normal guest exit | Normal guest exit | Exact ROM/EUSER executive map; physical behavior differs by feature and build |

All rows were rerun with the matrix script on 2026-10-08. A normal
AppArc close tests process lifetime, not every SDL renderer, audio or input
operation. The installed-SDK build and its own firmware run are separate
release gates; source-workspace results do not satisfy them.

## Runtime profiles

The RM-807 executive profile is selected only for its matching ROM/EUSER pair.
Other firmware uses the default executive map. A mismatched explicit profile
is rejected; ambient `EKA2L1_EXPERIMENTAL_SVC_PROFILE` cannot override selection.
Launch reports record the selected profile and mappings.

`Symbian::NativeAtomics64` requires selected EUSER 64-bit exports. RM-807,
RM-675 and RM-609 profiles provide them; RM-243 and RM-346 do not. Compatibility
must be checked against the actual firmware, rather than a product name alone.

Unknown font-server requests return `KErrNotSupported`. Inspect the guest exit
record even when the frontend exits zero: -4 maps to `RESOURCE_EXHAUSTED`, -5
to `UNIMPLEMENTED`, and panic records retain their native reason.

Emulator system-service coverage is incomplete. Firmware import, application
launch and full OS boot are different operations; test installation and the
required services on the target phone before deployment.
