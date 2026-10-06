# Build a Symbian Qt app

`examples/qt_app_classic` displays a full-screen Qt button. Tapping it closes the
application through the Qt event loop and runs the normal application
destructors. It uses guest Symbian Qt 4.8.1; the Qt 6 desktop libraries bundled
with the emulator serve its host interface and cannot provide guest widgets.

## Install the SDK and emulator

Follow [Getting started](../getting-started.md) to install `symbian-platform`
into a Python environment. [Install the native SDK archive](native-distributions.md)
for your host, then install and check the compatible emulator:

```sh
symbian emulator install
symbian emulator doctor
```

[Import your firmware](firmware.md) and select its local alias. This example
requires EKA2 firmware with guest QtCore and QtGui 4.8.1 installed. The SDK and
emulator distributions include neither firmware nor guest Qt DLLs. Do not
replace the Qt DLLs in another device's firmware with this example's imports.

## Prepare the example

The native SDK supplies the original QtCore/QtGui headers, complete import
libraries, compatibility settings and allocator hook. It also includes the
example source. After installing the SDK at `~/dev/symbian-sdk`, build it:

```sh
qt_project="$HOME/dev/symbian-sdk/examples/qt_app_classic"
symbian build --project "$qt_project" --output .symbian/qt-app
symbian inspect --format e32 .symbian/qt-app/qt_app_classic.exe
```

The application build is an ordinary CMake target:

```cmake
include(SymbianApp)
symbian_add_executable(qt_app_classic app.cc)
target_link_libraries(qt_app_classic PRIVATE Symbian::Runtime Symbian::QtGui)
```

`Symbian::QtGui` brings in QtCore. The SDK generates the needed E32 imports
from the libraries actually linked; applications need no symbol list or
startup files. `sdk-location.json` can select another installed native SDK.
If `Symbian::QtGui` is unavailable, install a native SDK containing guest Qt
support. Older SDK distributions do not provide this target.

The native SDK supplies Clang, LLD, CMake and Ninja on both hosts. From a
source checkout, set `qt_project="$PWD/examples/qt_app_classic"` to use the repository
copy instead.

## Run the button

Select your imported firmware and launch the application:

```sh
symbian app run --project "$qt_project" --firmware my-phone --backend dynarmic
```

The SDK stages the executable and its application resources, then opens the
full-screen button. It reads “Hello from Symbian Qt / Tap to close”; a tap
exits the guest application and closes the emulator. `--backend dyncom`
selects the other CPU backend.

To browse the firmware's application list instead, use
`symbian emu run --project "$qt_project" --firmware my-phone`, open
**Symbian Qt**, and close the emulator window when finished.

The application constructs its widgets after QApplication and connects the
button's signal to the application slot:

```cpp
QPushButton button(QString::fromUtf8("Hello from Symbian Qt\nTap to close"));
button.setWindowTitle(QString::fromUtf8("Symbian Qt"));
if (!QObject::connect(&button, SIGNAL(clicked()), &application, SLOT(quit()))) {
  return 3;
}
button.showFullScreen();
return application.exec();
```

The complete `app.cc` also selects raster graphics and Plastique, and disables
the S60 exit-animation signal. The emulator does not implement the Window
Server graphics plugins used by those animations. Keep those settings when
adapting this example for the emulator. This is an EKA2 widget example;
EKA1, other guest Qt versions, general Qt modules and physical-device operation
are outside its tested configuration.

## Package the application

The supplied `symbian.toml` declares the executable, menu caption and package:

```sh
symbian package --project "$qt_project" \
  --artifact .symbian/qt-app/qt_app_classic.exe --output .symbian/qt-package
symbian inspect --format sis .symbian/qt-package/qt_app_classic.sis
```

The package contains your executable and registration resources. It requires
the device's existing Qt installation. For signing and transfer, follow the
[application packaging and device guide](building.md#5-sign-the-sis).
