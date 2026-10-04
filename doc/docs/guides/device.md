# Connect a physical device

The SDK can inspect a handset over USB and stage an installable SIS on a
writable phone volume. These are host observations: a USB product name or
successful file copy does not prove which firmware is running or whether an
application will execute.

## 1. Identify the connection

Connect the phone in a mode that exposes a USB interface, then list candidates:

```sh
symbian device list
symbian device info
```

If several phones appear, use the selector printed by `device list` with
`device info --device SELECTOR`. The summary shows the observed USB roles and
mounted storage. The serial number is not printed. If you change the phone's
USB mode, list devices again because its selector may change.

## 2. Stage a package

With the handset exposing a writable mass-storage volume:

```sh
symbian device install --project ~/dev/hello_time --device SELECTOR
```

The command builds and checks an unsigned SIS, copies it into `Installs/` and
verifies the copied hash. It reports `awaiting-on-device-install`. Finish
transfers, safely eject the volume, then open the SIS on the handset and
approve installation there. A handset may reject the package or a required
system import. The SDK has no automated on-phone installation or launch
confirmation at this stage.

For PC Suite, MTP, OBEX and USB API details, see the [USB transport
reference](../reference/device-usb.md). [Packaging](packaging.md) explains
what goes into a SIS. Device recovery and firmware writing are outside this
workflow.
