# messaging_app_classic

This original messaging example installs an active scheduler, opens a
`CMsvSession` synchronously with a session observer and closes it. It links
`Symbian::Messaging`, which supplies `<msvapi.h>` and the frozen `msgs.dll`
interface. It does not read or change messages. Exit 1 means opening failed.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes this ARM target to IDEs. E32 build acceptance
alone does not prove the message server is available on every firmware.
The preserved RM-807/Dynarmic disposable emulator opened this session and
exited with guest type/reason `0/0`; no physical device was used.
