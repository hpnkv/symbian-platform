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
