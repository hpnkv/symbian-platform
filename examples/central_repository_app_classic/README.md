# central_repository_app_classic

This example uses original `<centralrepository.h>` and
`Symbian::CentralRepository` to open and read SIP timer T1 from repository
`0x101fed88`, key `0x01`. The source includes the original public
`<sipsdkcrkeys.h>` to get these identifiers rather than copying their values.
The preserved RM-807 fixture contains the repository. Exit 1 means opening
failed and exit 2 means reading failed or returned a nonpositive timer. The
program does not modify settings. This read exited zero in the preserved
RM-807/Dynarmic disposable emulator; other firmware remains untested.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes this ARM target to IDEs. Header compilation,
linking and E32 conversion do not establish service behavior on any firmware.
