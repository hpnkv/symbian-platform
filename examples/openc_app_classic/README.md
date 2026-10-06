# openc_app_classic

This original Open C example formats and parses a number through the device's
`libc.dll`. Link `Symbian::OpenC` for the public C headers and frozen libc
imports; its C and pthread prerequisites come from the SDK target. It does not
need an SDK wrapper around `snprintf`, `strtoul` or `strcmp`.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project also exposes the ARM source target to IDEs. E32 conversion
does not establish named-firmware execution or physical-device compatibility.
