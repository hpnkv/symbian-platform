# Symbian Qt button

This is a guest Symbian Qt 4.8.1 application, using original QtCore/QtGui
headers and DLL exports. It displays a button and exits when tapped.

Install `symbian-platform`, its native SDK and the compatible emulator. The SDK supplies
the original guest Qt headers and import libraries. Run from the repository root:

```sh
symbian build --project examples/qt_app --output .symbian/qt-app
symbian emu run --project examples/qt_app --firmware my-phone
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
