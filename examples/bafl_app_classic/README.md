# bafl_app_classic

This original BAFL example opens an `RFs` session and uses
`BaflUtils::FileExists` to check the preserved RM-807 fixture's public
`Z:\data\animations\startup.aac` file.
It also reads a 32-bit integer from an in-memory `RMemReadStream`. It links
only `Symbian::Bafl`; the SDK supplies the transitive FileServer and
StreamsNative imports. Exit 1 means the file-server session failed, exit 2
means the file was absent or inaccessible, and exit 3 means the stream read
failed. Exit 4 means the direct `RFs::Entry` check failed before BAFL ran.
The example does not modify files or settings. The named RM-807/Dynarmic
fixture passed this read; file availability on other firmware is unknown.

Select an installed native SDK in ignored `sdk-location.json`, then run
`cmake --preset symbian-pic` and `cmake --build .symbian/build/cmake`.
The root SDK project exposes this ARM target to IDEs. A successful build alone
does not establish file-server behavior on every firmware.
