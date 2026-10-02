# Physical device connection (development status)

`symbian device list` enumerates candidate Symbian handsets on macOS from the
IOService USB tree. It associates a mounted disk only when that disk is a child
of the same USB device; a volume label or a coincidental mount name is not
enough. `symbian device info [--device SELECTOR]` reports the descriptors and
storage that the host can actually observe. The serial number is never printed;
the selector contains a short hash of it, or the port location when no serial
is available. Selectors should be rediscovered after reconnecting a device.
With several candidates, specify the exact selector from `device list`.
The CLI shows a readable device and storage summary by default. Use
`--output-format=json` on `device list`, `device info` or other commands when
a script needs the unchanged structured response.

On 2026-10-02 the connected Nokia 808 was observed as USB vendor/product
`0421:05d0` with one writable mounted `S60` mass-storage disk and an existing
`Installs` directory. This identifies a USB product name, **not** the phone's
RM code, installed firmware, operating system version, free internal storage,
signing policy or application runtime compatibility. macOS
`system_profiler SPUSBDataType` returned an empty list for this attachment;
IOService and `diskutil` gave the consistent parent/volume association.

`symbian device install --project PROJECT [--device SELECTOR]` builds the
project through its selected SDK (when it has `symbian-project.json`), makes a
reproducible unsigned SIS, checks it with the native SIS reader, and stages it
as `Installs/NAME-SHA256PREFIX.sis` on one selected writable volume. Pass
`--volume diskN` if several volumes qualify. `--package FILE.sis` instead
stages an already built SIS. Staging verifies the copied SHA-256 and is
idempotent; it never overwrites different content, and it requires the package
size plus 16 MiB of free space. The structured result state
is `awaiting-on-device-install`, **not** `installed`. Finish other transfers,
safely eject the volume, and open the SIS on the handset to approve installation.
The handset may reject an unsigned package or unavailable imports. The SDK
does not yet observe the phone's installer registry or launch the application,
so user confirmation alone is not treated as automated installation evidence.
Projects with `[application]` in `symbian.toml` package registration and
localized caption resources for the application menu. Change `caption` and
`short_caption` there; `symbian init` supplies defaults. The visible SDK
includes the EPL-licensed `rcomp` host tool. The package reader checks all
three embedded file hashes and destinations. `rcomp -u` emits Unicode
resources, which EKA2L1's original AppArc parser decoded with the expected
menu captions. Its headless installer accepted and removed the resources on
both CPU backends. Older projects without `[application]`
remain single-EXE packages and may have no menu entry.

On 2026-10-02, after photo copying finished, the SDK staged a generated
ARMv5T portable app with verified Unicode menu resources at
`Installs/menu_v5-db4875221d6e.sis` on the observed phone volume. It verified
SHA-256 `db4875221d6ecabbe02bfba76c4201c5da5f8f0df298ae50f7f5050356e70db1`
after the copy and safely ejected `disk4`. The owner then opened the package
on the handset and reported successful installation with `menu_v5` visible
in the application menu. The owner also opened it and observed a response to
a tap. The CLI's result remains
`awaiting-on-device-install` because this confirmation is not collected by an
automated on-phone installer adapter. Application execution remains a
user-observed check; no device log, process trace or OS/RM identity was
collected.

Example from the workspace:

```sh
.venv/bin/python -m symbian.cli device list
.venv/bin/python -m symbian.cli device info --device usb:0421:05d0:…
.venv/bin/python -m symbian.cli device install \
  --project ~/dev/symbian-app-3 --device usb:0421:05d0:…
```

The default `device install` command is presently a staged installation flow
because the observed phone is in mass-storage mode. The [Gammu configuration
guide](https://docs.gammu.org/faq/config.html) describes Symbian remote access
through an on-phone Bluetooth applet; its [`install`
command](https://docs.gammu.org/gammu/) installs that applet rather than an
arbitrary SDK application. An MTP product ID or USB vendor ID alone cannot
establish an installer protocol; inspect the active interface before adding a
transport. No signing, PC Suite protocol, MTP transfer or direct installer
session has been validated here.

The public device model separates connection, volumes, capabilities and
installation state. A Linux adapter now joins sysfs USB ancestors to mounted
block devices through `/proc/self/mountinfo`; its synthetic test passes, but
it remains unvalidated with a real Linux-connected phone. Future screenshot
and debugging adapters should advertise
capabilities only when their transports provide them, with explicit process,
artifact and session identifiers. Firmware/recovery operations must remain
outside the agent-executable device API and require a separate human-governed
broker. No flash, erase, bootloader, partition, OTP, calibration or recovery
endpoint belongs in `symbian device`.
