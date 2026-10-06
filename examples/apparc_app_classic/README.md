# apparc_app_classic

This original AppArc example opens and closes an application-list session
through `RApaLsSession`. Link `Symbian::AppArc` for the public `apgcli.h`
header, its frozen `apgrfx.dll` import interface and SDK-managed dependencies.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes this ARM target to IDEs. Successful linking and
E32 conversion do not establish AppArc service availability on every firmware.
