# Build a real GUI app

The native Window Server counter in `examples/gui_app` is a compact route from
C++ source to a visible Symbian application. Window Server is the OS service
that owns windows and delivers drawing and pointer events. The example draws
four digits and three touch controls, then exits through the guest runtime.

## Build from an installed distribution

[Install the native SDK archive](native-distributions.md), then copy the
included counter application into your own project directory:

```sh
cp -R ~/dev/symbian-sdk/examples/gui_app ~/dev/gui_app
printf '%s\n' '{"sdk": "../symbian-sdk"}' > ~/dev/gui_app/sdk-location.json
symbian build --project ~/dev/gui_app --output ~/dev/gui_app/.symbian/build
symbian inspect ~/dev/gui_app/.symbian/build/gui_app.exe --format e32
symbian package --project ~/dev/gui_app \
  --artifact ~/dev/gui_app/.symbian/build/gui_app.exe \
  --output ~/dev/gui_app/.symbian/package
```

Edit `app.cc`, `window_server.cc`, `model.h` and `symbian.toml` in your copied
project. The selected SDK supplies the compiler, headers and libraries. Follow
[the application installation guide](building.md#5-sign-the-sis) to sign the SIS,
stage it and approve installation on a permissive device. Use the
[firmware guide](firmware.md) to configure an emulator separately.

## Build from a source checkout

The steps below also show how to prepare the SDK itself and inspect the
application's startup, compiler and emulator contracts. Run source-checkout
commands from the repository root.

| Step | Guide | You will have |
| --- | --- | --- |
| 1 | [Prepare the toolchain and public source](source-prerequisites.md) | Clang, LLD, Ninja and source inputs |
| 2 | [Understand the example](gui-architecture.md) | A map of startup, Window Server and packaging code |
| 3 | [Build and inspect](gui-build.md) | ARM ELF, E32 and host checks |
| 4 | [Run in the emulator](gui-emulator.md) | A disposable named-firmware session with real pixels and input |
| 5 | [Inspect symbols and debug](gui-debug.md) | Source navigation and ARM guest breakpoints |
| 6 | [Troubleshoot](gui-troubleshooting.md) | Common failures and diagnostics |
| 7 | [Investigate the guest C++ runtime](guest-runtime-source.md) | Runtime profile and native dependency context |

The [CLion guide](clion.md) explains IDE profiles and Run/Debug controls.
For device transfer and installer approval, use the [device guide](device.md).
Compiler or emulator success does not prove Nokia 808 compatibility.

## Incremental SDK inputs

The root CMake workspace prepares foreign headers, frozen import proxies and
patched Abseil sources under `.symbian/workspace-inputs`. Native libraries and
application sources remain ordinary CMake/Ninja targets.

Preparation tracks headers, import interfaces and Abseil independently. It
fingerprints input contents and watches tree membership, so added or removed
headers are detected too. It preserves unchanged output timestamps, keeping
Ninja's rebuilds limited to affected consumers. Owned `.cc` edits use Ninja's
normal compile dependencies.

Import proxies have individual content-addressed entries under
`.symbian/cache/import-proxies`. A new DEF/export selection, proxy generator,
relevant toolchain or compiler/linker identity gets a new verified entry. Cold
entries retain the two independent CMake builds and byte-for-byte comparison;
reuse checks artifact digests, native export metadata and recorded build inputs.
Concurrent preparers share an entry lock. Corrupt or missing entries rebuild.
The standalone import-proxy experiment still creates its complete local build
and evidence tree.

An Abseil patch or upstream source edit refreshes the patched Abseil view while
preserving unchanged files. It does not rebuild frozen proxies. Pinned source
and header digests are still enforced; changing a reviewed foreign interface
also requires updating its manifest. All caches are disposable and ignored by
version control.

Measured on the macOS arm64 development host on 12 October 2026:

| Operation | Elapsed | Work performed |
| --- | ---: | --- |
| Cold full preparation, comparison baseline | 239.2 s | 225 independently reproduced proxies, headers and Abseil |
| Current inputs, five steady-state runs | 0.95–1.18 s | Content-aware validation; no stage regenerated |
| One legacy DEF changed, two runs | 5.64–7.19 s | 1 proxy built, 224 reused; headers and Abseil unchanged |
| One Abseil patch changed, two runs | 1.56–1.59 s | Abseil refreshed; headers and imports unchanged |
| Native failure-handler archive, unchanged | 0.41–0.42 s | Ninja reports no work |
| One owned C++ source touched | 1.32 s | One translation unit compiled and its archive relinked |

The DEF control appended a comment without changing exports. The patch control
appended a newline without changing the patched source result. Both inputs were
restored exactly. These controls measure invalidation scope, including retaining
unchanged output timestamps; they do not predict arbitrary compile times. The
first preparation after the cache implementation changed took 7.17 s with all
225 proxies reused; the steady-state row excludes that refresh. A full CMake
reconfiguration still took about 20 s in the guest profile and is separate from
the incremental Ninja build timings above.

Raw logs and JSON measurements are retained under
`.symbian/workspace-cache-measurements/`; replay scripts are archived with the
12 October failure-handler and workspace-cache research evidence. Firmware
copying and emulator startup are absent from these build measurements.
