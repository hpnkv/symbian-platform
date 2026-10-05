# Build and install an application

A Symbian application build produces an ARM **ELF** with debug symbols and an
**E32** image that the Symbian loader reads. CMake and Ninja compile the source;
the SDK's converter checks the linked ELF and writes E32. A **SIS** installer is
a separate packaging result.

## 1. Build the starter

After [creating a project](projects.md), run:

```sh
symbian app build --project ~/dev/hello_time
```

The project selects the installed SDK and architecture. Build output and logs
stay in its ignored `.symbian/` directory. A missing import, unsupported
relocation or incompatible target setting fails the build instead of creating
an apparently usable image.

## 2. Inspect the output

The generated starter writes its executable to `.symbian/build/hello_time.exe`
inside the project:

```sh
symbian inspect --format e32 ~/dev/hello_time/.symbian/build/hello_time.exe
```

Inspect reports image metadata; it does not execute the application. Keep the
ELF as well: its DWARF symbols let ARM GDB map guest addresses back to your
source. The [Build a real GUI app](gui-build.md) shows concrete ELF and E32
paths for the maintained example.

## 3. Check it in an emulator

Import a local ROM/Z using the [firmware guide](firmware.md), then run:

```sh
symbian app run --project ~/dev/hello_time
```

An ARM ELF or converted E32 passing structural checks does not prove that the
Symbian loader accepts it. Run the application in the emulator and check its
native exit report; test installation and required services on the target device
before deployment.

## 4. Package the application

The starter already declares its executable identity, package version, menu
caption and icon in `symbian.toml`. Keep that identity for subsequent builds of
the same development application. Package the executable built in step 1:

```sh
symbian package --project ~/dev/hello_time \
  --artifact ~/dev/hello_time/.symbian/build/hello_time.exe \
  --output ~/dev/hello_time/.symbian/package
symbian inspect --format sis ~/dev/hello_time/.symbian/package/hello_time.sis
```

The SIS contains the executable and application-menu resources. The inspector
checks the package's integrity and embedded image. The generated icon and
captions give `hello_time` a launcher entry after installation. See
[packaging](packaging.md) to change those resources or add a CA bundle.

## 5. Sign the SIS

Create a local signing identity once. If you already have `developer` from
another project, reuse it and skip the creation command:

```sh
symbian signing create --identity developer --common-name "Local developer"
symbian signing sign ~/dev/hello_time/.symbian/package/hello_time.sis \
  --identity developer \
  --destination ~/dev/hello_time/.symbian/package/hello_time-signed.sis
symbian inspect --format sis \
  ~/dev/hello_time/.symbian/package/hello_time-signed.sis
```

Signing validates the package and verifies its signature before writing the new
file. The destination must not already exist; choose another filename for a
later signed build. The private key stays in the host's signing store. A
self-signed identity suits the permissive development device used here; it does
not grant additional Symbian capabilities. See [signing](signing.md) for an
existing certificate and key.

## 6. Stage the signed package on the phone

Connect the phone by USB in mass-storage or PC Suite mode, then identify it:

```sh
symbian device list
symbian device info --device SELECTOR
```

Replace `SELECTOR` with the value from `device list`. Stage the exact signed SIS
rather than rebuilding an unsigned package:

```sh
symbian device install --device SELECTOR \
  --package ~/dev/hello_time/.symbian/package/hello_time-signed.sis
```

The command copies to `Installs/` on a writable phone volume or MTP store and
verifies the transferred file's SHA-256. Its report includes `staged_path` and
`awaiting-on-device-install`. The staged name includes a digest, for example
`hello_time-012345abcdef.sis`; use the name in your report.

On macOS and Linux, close files on a mass-storage volume and safely eject it
with the host's file manager before switching the phone out of USB storage
mode. For PC Suite/MTP, let the transfer finish before disconnecting. If the
SDK cannot access the USB interface, follow the
[Linux USB setup](linux.md#4-usb-devices) or the
[device guide](device.md).

You can also copy `hello_time-signed.sis` with your host's file manager into an
`Installs` folder on the phone's exposed storage. Safely eject that volume
before opening the file on the handset.

## 7. Install and run on the handset

These steps assume a phone with a file browser and an installation policy that
accepts development packages:

1. Open the phone's file browser and find the SIS in `Installs` on the storage
   you selected, such as the mass-memory or memory-card drive.
2. Open the SIS, review the application's name and publisher, and accept the
   installer prompts. Choose a writable install drive if prompted.
3. Open the application menu, find **hello_time**, and launch it. Some devices
   place newly installed applications in an **Applications** folder.
4. Check that the clock advances, taps add text, **Clear** clears it, and
   **Exit** closes the application. Relaunch it to check startup again.

The SDK's staging report stops before handset installation. These on-device
checks establish whether that phone has the required services and accepts the
application. Use the phone's application manager to remove the development app.
EKA1 phones use a separate [process profile](eka1.md); this GUI starter requires
EKA2 and its selected runtime imports.

For a custom source graph, linker script, reproducibility report or low-level
PIC probe, continue to the [E32 build pipeline](../reference/e32-build-pipeline.md).
