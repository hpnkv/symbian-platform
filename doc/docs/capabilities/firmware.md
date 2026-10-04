# Firmware and emulator compatibility

These are bounded emulator and import observations for named firmware samples. They do not predict physical-device behavior. See the [firmware guide](../guides/firmware.md) to select a local image.

## Actual contracts and remaining gaps

The C++ runtime supplied by this SDK remains enabled regardless of historical
Symbian feature lists. Requirements are actual ARM EABI/EKA2 E32-V startup,
import ordinals and the services an application uses. EKA1 dumps import, but
the current starter fails before launch with a specific missing EKA2 ABI error;
an EKA1 startup/import adapter has not been implemented. Building an application
without firmware remains possible.

The GUI requires EUSER, WS32 and GDI. Missing DLLs fail before process creation.
The starter uses the default font, adapts down to 176x208 logical pixels and
supports touch, Enter/centre, Backspace and Clear/Exit softkeys. Its model keeps
twelve rows; the smallest displays show only the newest rows that fit.
Full orientation handling and all vendor/server ABIs remain unfinished.

The RM-807 executive workaround is auto-selected only for its exact verified
ROM/EUSER pair. Other dumps use the default executive map. An explicit mismatched
profile is rejected; ambient `EKA2L1_EXPERIMENTAL_SVC_PROFILE` cannot override
the resolver. Each launch report records the effective profile and mappings.

The optional `Symbian::NativeAtomics64` runtime has been executed with the
imported RM-807, RM-675 (C7) and RM-609 (E6) ROMs on both emulator CPU
backends. Their EUSER 64-bit entry points match and use LDREXD/STREXD. The
imported RM-243 (6120c) and RM-346 (E71) EUSERs lack those EABI ordinals;
selecting this profile for them is unsupported. This is firmware capability
evidence, not a claim about every device of the same model or physical phone.

Unknown font-server requests return `KErrNotSupported` and remain logged;
they no longer leave a synchronous client waiting forever. This supplies an
error contract, not an implementation of the missing font operation. Native
guest exits are checked even when the frontend exits zero. A guest reason -4
is `RESOURCE_EXHAUSTED`; -5 is `UNIMPLEMENTED`; panics and other failures retain
their actual reason and evidence path.

See the [development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md) and [research log](https://github.com/hpnkv/symbian-platform/blob/main/.dev/research-log.md) entries for tested devices,
screenshots, CPU backend coverage and retained failure controls. Import success
does not establish application execution, full OS boot or physical compatibility.
