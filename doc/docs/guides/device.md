# Connect a physical device

The SDK can inspect a handset over USB and stage an installable SIS through a
writable phone volume or PC Suite MTP. These are host observations: a USB product name or
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

With the handset in mass-storage or PC Suite mode:

```sh
symbian device install --project ~/dev/hello_time --device SELECTOR
```

The command builds and checks an unsigned SIS, stages it in `Installs/`, and
verifies its SHA-256. A mounted volume is used when available; otherwise the
SDK checks the device's writable MTP store and folder before transfer. It
reports `awaiting-on-device-install`. For a mounted volume, finish transfers
and safely eject it. Then open the SIS on the handset and approve installation
there. A handset may reject the package or a required
system import. The SDK has no automated on-phone installation or launch
confirmation at this stage.

For PC Suite, MTP, OBEX and USB API details, see the [USB transport
reference](../reference/device-usb.md). [Packaging](packaging.md) explains
what goes into a SIS. Device recovery and firmware writing are outside this
workflow.

To stage an already signed package, supply `--package` so the SDK transfers
that file without rebuilding:

```sh
symbian device install --device SELECTOR \
  --package ~/dev/hello_time/.symbian/package/hello_time-signed.sis
```

Follow [Build and install an application](building.md#7-install-and-run-on-the-handset)
for the complete `hello_time` packaging, signing and on-phone sequence.
