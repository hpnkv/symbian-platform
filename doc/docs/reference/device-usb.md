# USB device inspection and host transport

This reference covers the detailed host USB surface and observed device evidence. Begin with [Connect a device](../guides/device.md) for the paced workflow.

`symbian device list` enumerates candidate Symbian handsets and joins
host storage or serial-port associations from macOS IOService or Linux sysfs.
`device info` always reads the active USB function map through the SDK's
statically linked libusb backend. The OS inventory supplies host associations;
libusb supplies the shared descriptor, endpoint, and transfer path. The OS
adapter associates a mounted disk only when that disk is a child of the same
USB device; a volume label or a coincidental mount name is not
enough. `symbian device info [--device SELECTOR]` reports the descriptors and
storage that the host can actually observe. The serial number is never printed;
the selector contains a short hash of it, or the port location when no serial
is available. Selectors should be rediscovered after reconnecting a device.
With several candidates, specify the exact selector from `device list`.
The CLI shows a readable device and storage summary by default. Use
`--output-format=json` on `device list`, `device info` or other commands when
a script needs the unchanged structured response.

`device info` also reports each host USB interface's configuration, number,
class, subclass and protocol, plus any macOS serial-port path descended from
that USB device. The human view leads with USB-standard roles, device-declared
interface names, endpoint counts, alternate settings and bound host drivers;
supported terminals color the roles and probe results. JSON retains the raw
codes. A label such as `PC Suite Services` is device-declared metadata, not
proof that its application protocol was opened. Zero endpoints describes the
listed alternate setting only. `interface_profile` describes class evidence
only. The standard roles follow the
[USB-IF class-code registry](https://www.usb.org/defined-class-codes) and
[CDC definitions](https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/cdc.h).
A `usb-serial` identity anchor is a truncated hash of the vendor ID and USB
serial, independent of USB product ID. A `port-location` anchor is weaker and
cannot prove the same handset after a mode change. Neither prints the serial.

For the observed Nokia 808 `0421:05d1` CDC ACM configuration, `device info`
also sends four bounded, read-only AT queries by default: `AT+GCAP`,
`AT+CGMI`, `AT+CGMM`, and `AT+CGMR`. Use `--no-protocol` to show USB evidence
without opening the port. `reported_identity` retains the source label;
firmware revision, date and RM code are parsed from the phone's AT reply,
not authenticated against preserved firmware. No IMEI, IMSI, phonebook,
message, dialing or settings command is issued. The port is opened for at most five one-second exchanges by default, or
seven with `--at-status`, including the `AT` handshake. Its host terminal
settings are restored.
The [ETSI AT command specification](https://www.etsi.org/deliver/etsi_ts/127000_127099/127007/14.09.00_60/ts_127007v140900p.pdf)
defines the identity queries; [ITU V.250](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-V.250-200307-I%21%21PDF-E&lang=e&type=items)
defines `+GCAP`. These AT replies prove basic modem AT access. The separate
OBEX session test is documented below; a Symbian debugger transport remains
unvalidated.

To check a handset-side USB mode change, save a baseline before switching:

```sh
.venv/bin/python -m symbian.cli device mode begin \
  --device SELECTOR --ticket .symbian/phone-mode.json
# Select PC Suite / Nokia Suite mode on the handset.
.venv/bin/python -m symbian.cli device mode verify \
  --ticket .symbian/phone-mode.json
```

The ticket is created once with owner-only permissions and no raw serial.
`verify` requires the same serial-derived anchor, then compares product ID and
interface descriptors. It reports `unchanged`, `device-unavailable`, or
`usb-transition-observed`. It does not label a changed USB configuration as a
verified PC Suite protocol session. The CLI cannot select the mode on the
phone; the owner makes that choice on the handset. No phone log, process,
screenshot or remote debugging transport has been validated through this
interface yet.

On 2026-10-02 the connected Nokia 808 was observed as USB vendor/product
`0421:05d0` with one writable mounted `S60` mass-storage disk and an existing
`Installs` directory. This identifies a USB product name, **not** the phone's
RM code, installed firmware, operating system version, free internal storage,
signing policy or application runtime compatibility. macOS
`system_profiler SPUSBDataType` returned an empty list for this attachment;
IOService and `diskutil` gave the consistent parent/volume association.
The host interface descriptor was `08/06/50` (mass storage). After the owner
selected PC Suite mode on the handset, the same USB location presented
`0421:05d1`, 18 interfaces (still-image and communications/data classes), no
mounted disk, and a macOS CDC ACM port at `/dev/cu.usbmodem141202`. This is a
host-observed USB transition and an owner-reported phone-side selection. The
port has not answered a validated PC Suite or debugging protocol handshake.
Its interface names include `MTP` (still-imaging/PTP class), `SYNCML-SYNC`,
`PC Suite Services`, `SYNCML-DM`, `Haptics Bridge`, `UsbPnComm` and `LCIF_Alt0`.
Only the CDC ACM data interface has a host serial binding in this inventory.
It did answer the AT handshake and read-only identity queries: manufacturer
`Nokia`, model `Nokia 808 PureView`, revision
`113.010.1508 2013-01-02 RM-807 (c) Nokia`, and `+GCAP: +CGSM,+DS,+W`.
These are handset-reported values. OS build, product code, installer state and
debugging capabilities remain unknown.


## Inspecting the 808 in PC Suite mode

With PC Suite / Nokia Suite selected on the handset, use the exact selector from
`device list` if more than one candidate is connected. The commands below run
in order; each protocol step is opt in. JSON output is available with
`--output-format=json`.

```sh
.venv/bin/python -m symbian.cli device info --no-protocol
.venv/bin/python -m symbian.cli device info --at-status
.venv/bin/python -m symbian.cli device info --mtp
.venv/bin/python -m symbian.cli device info --mtp-list 3
.venv/bin/python -m symbian.cli device info --obex-connect
```

The first command maps all alternate settings, endpoint directions and transfer
types, CDC unions, and interface associations without claiming an interface.
`--at-status` adds fixed `AT+CBC` and `AT+CSQ` queries to the identity probe.
The returned signal code `99` means unavailable, not a measured signal
strength. `--mtp` opens a PTP/MTP session, reads device and storage metadata,
and closes it. `--mtp-list 3` additionally fetches at most three root object
names per storage; it does not transfer file contents. Names are private device
data and should be kept out of shared logs. `--obex-connect` claims only the
PC Suite data interface, selects alternate setting 1, sends an OBEX Connect
with the PC Suite FTP target, then sends Disconnect with the returned
Connection ID and restores alternate setting 0. A successful Connect alone
does not prove file browsing or any debugging command works.

The native host SDK includes `symbian/host/libusb-1.0.a`,
`symbian/host/include/libusb.h`, and `symbian/licenses/libusb-COPYING`. The
extension links the same libusb version statically, so the inspection CLI has
no separate runtime libusb package dependency. The library is LGPL 2.1 or
later; the pinned source is libusb 1.0.30 in the wheel dependency bootstrap.
The SDK installer copies the archive and header into `lib/host`.

For a custom Python USB flow, `symbian.device.usb.open_selected()` returns a
context-managed native session. `AsyncUsbSession` drives libusb from the
current asyncio loop and returns `Future[UsbCompletion]`. The completion is a
Pydantic model with an ID, status, byte count, and IN bytes:

```python
import asyncio
from symbian.device.usb import AsyncUsbSession, open_selected

async def main():
    async with AsyncUsbSession(open_selected()) as usb:
        result = await usb.control_in(0x80, 0, 0, 0, 2)
        assert result.status == "completed"
        print(len(result.data))

asyncio.run(main())
```

The native session also exposes synchronous bulk, interrupt, and control
transfers. Its explicit `submit_*` methods return transfer IDs;
`handle_events()` returns typed native completions for callers driving libusb
themselves. `claim`, `set_alternate`, `release`, `cancel`, `poll_fds`,
`next_timeout_ms`, and `descriptors` are available. The inspection API returns
Pydantic `UsbProbe` models, and `list_devices()` returns Pydantic
`UsbDeviceDescriptor` models. Use `model_dump(mode="json")` at an output
boundary. Default or unobserved optional fields are omitted.

`AsyncUsbSession` watches libusb file descriptors and a bounded timeout. Its
native transfer Futures use A11's `FutureToPython` bridge; cancelling a Python
Future cancels the libusb transfer. The libusb callback only records bounded
completion data; Python result conversion and user continuations run after the
callback returns. Keep compute-heavy continuations off the event loop. Native
admission stops at 32 pending plus undrained results (64 KiB per transfer).
Libusb already supports asynchronous writes, so this path needs no fiber or
second scheduler.

On the connected 808, the native map identified MTP on interface 0 and the
`PC Suite Services` CDC union 8→9. The read-only MTP session returned two
storage records (`Mass memory` and `Phone memory`), and a bounded root listing
succeeded for both. The OBEX FTP target returned `0xA0` to Connect and,
with its Connection ID supplied, `0xA0` to Disconnect. These observations are
for this attachment and firmware; file transfer, SyncML, PC Suite commands,
and debugger access have not been tested.

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

The default `device install` command stages only when the handset exposes a
writable mass-storage volume. The [Gammu configuration
guide](https://docs.gammu.org/faq/config.html) describes Symbian remote access
through an on-phone Bluetooth applet; its [`install`
command](https://docs.gammu.org/gammu/) installs that applet rather than an
arbitrary SDK application. An MTP product ID or USB vendor ID alone cannot
establish an installer protocol; inspect the active interface before adding a
transport. The PC Suite OBEX handshake is documented above; no MTP file
transfer or direct installer session has been validated here.

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
