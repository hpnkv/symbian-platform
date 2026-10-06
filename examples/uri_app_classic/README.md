# uri_app_classic

This example uses original `<uri8.h>` and `Symbian::Uri` to parse a fixed
HTTPS URI and compare its host. It links the frozen `inetprotutil.dll` import
interface. It opens no network connection and changes no device state. Exit 1
means parsing failed and exit 2 means the host did not match.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes this ARM target to IDEs. URI parsing does not
establish HTTP transport or browser integration on any firmware.
The preserved RM-807/Dynarmic emulator parsed the URI and exited with guest
type/reason `0/0`; physical-device behavior remains unknown.
