# SDK examples

New capability examples that call original Symbian or Nokia APIs use the
`_classic` suffix on their directory, CMake target and executable name. This
leaves a clear name for a later example using the SDK's own API for the same
task. The current classic examples cover audio, bitmaps, image decoding,
vibration, Qt widgets and Qt modules. `linking_app` demonstrates SDK build and
packaging mechanics, including an in-project static library and DLL.

Every example is registered in the root CMake project for IDE indexing. Its
standalone `CMakePresets.json` also accepts a selected installed SDK through
the ignored `sdk-location.json` file. Building and E32 conversion show
compiler and image-format acceptance; runtime compatibility depends on the
selected firmware and services.
