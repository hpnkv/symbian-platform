# USB device inspection and host transport

This reference describes USB inspection, asynchronous transfers and SIS staging. Begin with [Connect a device](../guides/device.md) for the paced workflow.

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
listed alternate setting only. `interface_profile` describes the USB class only. The standard roles follow the
[USB-IF class-code registry](https://www.usb.org/defined-class-codes) and
[CDC definitions](https://github.com/torvalds/linux/blob/master/include/uapi/linux/usb/cdc.h).
A `usb-serial` identity anchor is a truncated hash of the vendor ID and USB
serial, independent of USB product ID. A `port-location` anchor is weaker and
cannot prove the same handset after a mode change. Neither prints the serial.

For the observed Nokia 808 `0421:05d1` CDC ACM configuration, `device info`
also sends four bounded, read-only AT queries by default: `AT+GCAP`,
`AT+CGMI`, `AT+CGMM`, and `AT+CGMR`. Use `--no-protocol` to show USB descriptors
without opening the port. `reported_identity` retains the source label;
firmware revision, date and RM code are parsed from the phone's AT reply,
not authenticated against preserved firmware. No IMEI, IMSI, phonebook,
message, dialing or settings command is issued. The port is opened for at most five one-second exchanges by default, or
seven with `--at-status`, including the `AT` handshake. Its host terminal
settings are restored.
The [ETSI AT command specification](https://www.etsi.org/deliver/etsi_ts/127000_127099/127007/14.09.00_60/ts_127007v140900p.pdf)
defines the identity queries; [ITU V.250](https://www.itu.int/rec/dologin_pub.asp?id=T-REC-V.250-200307-I%21%21PDF-E&lang=e&type=items)
defines `+GCAP`. These queries provide modem-reported identity. They do not supply a Symbian
debugger transport.

To check a handset-side USB mode change, save a baseline before switching:

```sh
symbian device mode begin \
  --device SELECTOR --ticket .symbian/phone-mode.json
# Select PC Suite / Nokia Suite mode on the handset.
symbian device mode verify \
  --ticket .symbian/phone-mode.json
```

The ticket is created once with owner-only permissions and no raw serial.
`verify` requires the same serial-derived anchor, then compares product ID and
interface descriptors. It reports `unchanged`, `device-unavailable`, or
`usb-transition-observed`. It does not label a changed USB configuration as a
verified PC Suite protocol session. The CLI cannot select the mode on the
phone; the owner makes that choice on the handset.

## Inspecting the 808 in PC Suite mode

With PC Suite / Nokia Suite selected on the handset, use the exact selector from
`device list` if more than one candidate is connected. The commands below run
in order; each protocol step is opt in. JSON output is available with
`--output-format=json`.

```sh
symbian device info --no-protocol
symbian device info --at-status
symbian device info --mtp
symbian device info --mtp-list 3
symbian device info --obex-connect
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

`symbian device install --project PROJECT [--device SELECTOR]` builds the
project through its selected SDK (when it has `symbian-project.json`), makes a
reproducible unsigned SIS, checks it with the native SIS reader, and stages it
as `Installs/NAME-SHA256PREFIX.sis` on one selected writable volume or, in PC
Suite mode, on a writable MTP store with an unambiguous `Installs` folder.
Pass `--volume diskN` if several mounted volumes qualify. `--package FILE.sis`
instead stages an already built SIS. Staging verifies the readback SHA-256 and is
idempotent; it never overwrites different content, and it requires the package
size plus 16 MiB of free space. MTP uploads are limited to 16 MiB. The structured result state
is `awaiting-on-device-install`, **not** `installed`. Finish other transfers,
safely eject a mounted volume, and open the SIS on the handset to approve installation.
The handset may reject an unsigned package or unavailable imports. The SDK
does not yet observe the phone's installer registry or launch the application,
so staging does not change the result to `installed`.
Projects with `[application]` in `symbian.toml` package registration and
localized caption resources for the application menu. Change `caption` and
`short_caption` there; `symbian init` supplies defaults. The visible SDK
includes the EPL-licensed `rcomp` host tool. The package reader checks embedded file hashes and destinations. Older projects
without `[application]` remain executable-only packages and may have no menu entry.

If the phone lists an `Installs` child but refuses its metadata, the MTP
stager skips that unreadable handle. It reports the count as
`unreadable_children` when nonzero, uses a digest-derived filename to avoid
replacing another package, and still requires a complete readback match for
the selected SIS. A USB disconnect can clear stale MTP listings; retry after
the device reappears in `symbian device list`.

For a selected device:

```sh
symbian device list
symbian device info --device usb:0421:05d0:…
symbian device install \
  --project ~/dev/symbian-app-3 --device usb:0421:05d0:…
```

The [Gammu configuration
guide](https://docs.gammu.org/faq/config.html) describes Symbian remote access
through an on-phone Bluetooth applet; its [`install`
command](https://docs.gammu.org/gammu/) installs that applet rather than an
arbitrary SDK application. An MTP product ID or USB vendor ID alone cannot
establish a writable store or installer protocol. The reusable
`symbian.device.mtp.stage_sis` adapter and native `StageMtpSis` API re-check
the serial anchor, interface, required operations, writable store, folder,
free space and existing filename before writing. The device installer is
still a separate human action.

On Linux, mounted-volume association uses sysfs USB ancestry and
`/proc/self/mountinfo`. The SDK provides no phone screenshot, process-debug,
flashing, erasure, bootloader, partition, OTP, calibration or recovery endpoint.
