# zlib_app_classic

This app compresses and decompresses a fixed string with source-built zlib
1.3.1, then verifies the bytes and CRC. It links
`Symbian::PortableZlib` plus Window Server/GDI for its result screen; the SDK
supplies zlib's Open C and runtime dependencies.
No file access or firmware zlib DLL is required.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes the same target to IDEs. Building and E32
conversion establish compiler and format acceptance; execute in a named
firmware fixture to establish runtime behavior for that fixture.
