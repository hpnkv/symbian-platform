# png_app_classic

This example writes one RGBA pixel to a PNG in memory with source-built
libpng 1.6.53, reads it back and checks the decoded bytes. It links
`Symbian::PortablePng` plus Window Server/GDI for its result screen; the SDK
supplies portable zlib, Open C and runtime dependencies. It does not access
files or import a firmware image codec.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes the target to IDEs. Compiler/E32 acceptance and
named-firmware execution are separate checks.
