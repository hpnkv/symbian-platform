# Build a Symbian Qt app

`examples/qt_app` displays a full-screen Qt button. Tapping it closes the
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

Obtain the application source and the original Qt headers. You do not need to
build desktop Qt or install Nokia's Windows tools:

```sh
git clone https://github.com/hpnkv/symbian-platform.git
git clone --depth 1 --branch v4.8.1 https://github.com/qt/qt.git qt-4.8.1
cd symbian-platform
python examples/qt_app/prepare.py --qt-source ../qt-4.8.1
symbian build --project examples/qt_app --output .symbian/qt-app
symbian inspect --format e32 .symbian/qt-app/qt_app.exe
```

Preparation copies original QtCore/QtGui headers and their license notices,
uses the original Qt allocator hook, and generates selected ordinal imports
with the SDK's native proxy implementation. Generated inputs stay in the
example's ignored `.symbian/qt` directory. `sdk-location.json` selects the
installed native SDK; `--sdk /path/to/sdk` chooses another installation.
The helper downloads the original EUSER export definition when needed;
`--kernel-def /path/to/euseru.def` supports preparation without that download.

On Debian or Ubuntu, install the host utilities used during preparation:

```sh
sudo apt-get update
sudo apt-get install git perl cmake ninja-build
```

On macOS, use `brew install cmake ninja`; Git and Perl come with the system
development tools. Clang and LLD come from the native SDK on both hosts.

## Run the button

Select your imported firmware and stage the registered application:

```sh
symbian emu run --project examples/qt_app --firmware my-phone --backend dynarmic
```

Open **Symbian Qt** in the emulator's application list. The button reads
“Hello from Symbian Qt / Tap to close”; a tap exits the guest application.
Close the emulator window when finished. `--backend dyncom` selects the
other CPU backend.

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
symbian package --project examples/qt_app \
  --artifact .symbian/qt-app/qt_app.exe --output .symbian/qt-package
symbian inspect --format sis .symbian/qt-package/qt_app.sis
```

The package contains your executable and registration resources. It requires
the device's existing Qt installation. For signing and transfer, follow the
[application packaging and device guide](building.md#5-sign-the-sis).
