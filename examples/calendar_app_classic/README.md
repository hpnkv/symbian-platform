# calendar_app_classic

This example opens an original `CCalSession`, lists calendar file names and
closes the session without changing any entries. It links
`Symbian::Calendar`; the SDK supplies the original frozen
`calinterimapi.dll` imports and supporting targets. Exit 1 means session
creation failed and exit 2 means listing failed.
The preserved RM-807/Dynarmic disposable emulator completed this operation
with guest type/reason `0/0`; other firmware and physical hardware are untested.

The selected frozen interface comes from the default branch of the original
`calinterimapi.mmp`. That MMP also names a different DEF under
`SYMBIAN_CALENDAR_ENHANCEDSEARCHANDSORT`, so Belle FP2 ABI equivalence needs
separate verification. Compile/link/E32 acceptance does not establish a
calendar server or data access on every firmware.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes this ARM target to IDEs.
