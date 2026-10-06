# freetype_app_classic

This example loads a tiny original BDF test font embedded in its source and
renders the letter A through source-built FreeType 2.13.2. It links
`Symbian::PortableFreeType` plus Window Server/GDI to show the glyph and its
result; the SDK supplies FreeType's Open C and runtime dependencies.
The font bytes are part of this example, so no firmware font service or file
access is needed.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes the target to IDEs. Compiler/E32 acceptance and
named-firmware execution are separate checks.
