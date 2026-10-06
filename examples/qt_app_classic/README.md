# Symbian Qt button

This is a guest Symbian Qt 4.8.1 application, using original QtCore/QtGui
headers and DLL exports. Run shows a QtCore `QByteArray` result in a QtGui
window; Close exits through the Qt event loop.

Open the example directory in CLion and choose **App Run** or **App Debug**.
The shared **qt_app_classic Standalone Run/Debug** configurations live in `.idea/runConfigurations/`,
use the `symbian-pic` profile and
launch the selected SDK through `sdk-run` / `sdk-debug`. Set `sdk-location.json`
to your native SDK directory and select firmware with `symbian emu configure`.
Debug discovers ARM GDB in the selected SDK or on PATH; `SYMBIAN_GDB` overrides
the executable. Its endpoint is localhost:24702. Stop closes the owned emulator.

When opening the SDK repository root, use **Qt App Run** or **Qt App Debug**.
Run uses the host `debug` profile and `qt_app_classic_run` executable; both configurations
rebuild the example and ARMv6 SDK libraries from repository sources. They ignore
the example's installed SDK selector. Root Debug uses `.run/qt_app_classic-debug` and
symbols in `.symbian/workspace-apps/qt_app_classic`. Prepare source dependencies and
select firmware for the example; supply ARM GDB on PATH or with `SYMBIAN_GDB`.

Install `symbian-platform`, its native SDK and the compatible emulator. The SDK supplies
the original guest Qt headers and import libraries. Run from the repository root:

```sh
symbian build --project examples/qt_app_classic --output .symbian/qt-app
symbian emu run --project examples/qt_app_classic --firmware my-phone
```

Open **Symbian Qt** in the application list. Your imported EKA2 firmware must
provide QtCore and QtGui 4.8.1; the emulator's host Qt libraries cannot serve
as guest libraries. Raster graphics, Plastique and disabled S60 exit animations
avoid unsupported emulator graphics-plugin paths. Physical-device operation
and other Qt versions are untested.

See the [complete guide](../../doc/docs/guides/qt-app.md) for installation,
Linux/macOS dependencies, packaging and restrictions. The SDK retains Qt's
LGPL notices beside its headers and allocator hook. Applications link
`Symbian::QtGui` and use standard `main`; the SDK owns platform startup,
compiler settings and imports.
