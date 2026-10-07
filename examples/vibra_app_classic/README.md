# vibra_app_classic

Configure with an installed SDK selected in ignored `sdk-location.json`:

```json
{"sdk": "/path/to/native-sdk"}
```

```sh
cmake --preset symbian-pic
cmake --build .symbian/build/cmake
```

The root SDK project exposes this target and its ARM compile commands.
Build and E32 validation are distinct from execution on a selected firmware.
The HWRM client requires an active scheduler in the calling thread; the Run
handler installs one for the duration of the vibration request.
