# Symbian Qt button

This is a guest Symbian Qt 4.8.1 application, using original QtCore/QtGui
headers and DLL exports. It displays a button and exits when tapped.

Install `symbian-platform`, its native SDK and the compatible emulator. Obtain
the original Qt 4.8.1 sources, then run from the repository root:

```sh
python examples/qt_app/prepare.py --qt-source /path/to/qt-4.8.1
symbian build --project examples/qt_app --output .symbian/qt-app
symbian emu run --project examples/qt_app --firmware my-phone
```

Open **Symbian Qt** in the application list. Your imported EKA2 firmware must
provide QtCore and QtGui 4.8.1; the emulator's host Qt libraries cannot serve
as guest libraries. Raster graphics, Plastique and disabled S60 exit animations
avoid unsupported emulator graphics-plugin paths. Physical-device operation
and other Qt versions are untested.

See the [complete guide](../../doc/docs/guides/qt-app.md) for installation,
Linux/macOS dependencies, preparation, packaging and restrictions. The helper
retains Qt's LGPL notices beside its generated headers and allocator hook;
the original Qt source and generated inputs stay outside version control.
