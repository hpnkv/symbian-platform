# jpeg_app_classic

This example encodes one RGB pixel as a JPEG in memory with source-built IJG
libjpeg 8c, then decodes it and checks the image dimensions and color. Link
only `Symbian::PortableJpeg`; the SDK supplies Open C and runtime dependencies.
It does not access files or import a firmware image codec.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes the target to IDEs. Compiler/E32 acceptance and
named-firmware execution are separate checks.
