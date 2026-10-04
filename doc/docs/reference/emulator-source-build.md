# Build the emulator from source

The application guides use a prepared EKA2L1 installation. Build the pinned
research emulator only when investigating emulator behavior, guest debugging
or a missing system service. EKA2L1 is an external GPL project; keep its
checkout and build products outside version control and preserve its licenses.

## Source and patches

The maintained source checkpoint is EKA2L1 commit
`2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8`. The ordered local patches
under `research/eka2l1/` add isolated instance roots, guarded firmware import,
loopback guest debugging and bounded research controls. Read the
[research build notes](https://github.com/hpnkv/symbian-platform/blob/main/.dev/research/eka2l1.md)
before applying them to a fresh checkout. The existing workspace may already
have these patches; inspect it before changing it.

The research frontend needs Qt and an FFmpeg build compatible with the host.
The pinned source tree contains `linux_x64-build.sh` for x86_64; it has no
maintained Linux aarch64 FFmpeg script here. Use the same pinned checkout and
local patch sequence on either host. The configured build also creates
historical checksum and validator executables as separate oracles; their
algorithms are not linked into the SDK's native format implementation.

=== "macOS"

    The tested Apple Silicon setup uses Homebrew Qt and the pinned ARM64 FFmpeg
    script:

    ```sh
    brew install qtbase qttools qtsvg
    (cd research/upstream/EKA2L1/src/external/ffmpeg && sh macos_arm64-build.sh)
    cmake -S research/upstream/EKA2L1 -B build/eka2l1 -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
      -DEKA2L1_ENABLE_QT_CAMERA=OFF -DEKA2L1_SCRIPTING_LUA=OFF \
      -DCMAKE_PROJECT_EKA2L1_INCLUDE="$PWD/research/eka2l1/project-tests.cmake"
    cmake --build build/eka2l1 -j 8 --target eka2l1_qt
    ```

=== "Linux x86_64 (provisional)"

    Install Qt 6 development packages and other dependencies required by the
    [pinned EKA2L1 source](https://github.com/EKA2L1/EKA2L1). Verify the
    version of each tool and library on the host; this command sequence has
    not completed a Linux GUI build in this project.

    ```sh
    (cd research/upstream/EKA2L1/src/external/ffmpeg && sh linux_x64-build.sh)
    cmake -S research/upstream/EKA2L1 -B build/eka2l1 -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DEKA2L1_ENABLE_QT_CAMERA=OFF -DEKA2L1_SCRIPTING_LUA=OFF \
      -DCMAKE_PROJECT_EKA2L1_INCLUDE="$PWD/research/eka2l1/project-tests.cmake"
    cmake --build build/eka2l1 -j 8 --target eka2l1_qt
    ```

    On aarch64 Linux, first establish an FFmpeg build matching that host;
    the x86_64 script is not an aarch64 recipe.

## Isolated instance

The patched frontend accepts an explicit data root. Use a fresh root for each
run and do not launch the preserved firmware baseline directly:

=== "macOS"

    ```sh
    export SYMBIAN_EMULATOR="$PWD/build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
    EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
    ```

=== "Linux (provisional)"

    The pinned CMake target sets its executable output to `bin/eka2l1_qt`:

    ```sh
    export SYMBIAN_EMULATOR="$PWD/build/eka2l1/bin/eka2l1_qt"
    export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
    EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
    ```

The root holds `config.yml`, device data, writable drives and logs. On macOS, a direct manual launch may become the foreground app; the SDK
Run supervisor uses a separate background-window policy. Linux window focus
and managed GUI input remain unverified. The supervisor still owns and cleans
up its disposable process. Return to
[Run the GUI example](../guides/gui-emulator.md) for the application task.

The [development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
records which firmware and CPU backend controls passed. Emulator execution
cannot establish Nokia 808 compatibility.
