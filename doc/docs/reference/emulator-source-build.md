# Build the emulator from source

The application guides use a prepared EKA2L1 installation. Build the pinned
research emulator only when investigating emulator behavior, guest debugging
or a missing system service. EKA2L1 is an external GPL project; keep its
checkout and build products outside version control and preserve its licenses.

## Acquire and build

The maintained driver acquires commit
`2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8`, its recursive submodules and the
SDK patches in a disposable workspace. It checks source state before reuse.
Build FFmpeg and SDL from their pinned sources, then the Qt frontend, firmware
importer and native tests. The same driver supports both host architectures.

=== "macOS"

    Install the build tools and Qt modules:

    ```sh
    brew install cmake ninja nasm pkg-config qtbase qttools qtsvg
    python3 scripts/build_emulator.py all \
      --workspace .symbian/emulator-build --qt-prefix "$(brew --prefix)"
    ```

    Homebrew uses separate module prefixes; pass its common prefix so CMake
    finds both Qt base and LinguistTools. A distribution built for an older
    macOS version also needs Qt and dependency binaries built for that version.

=== "Linux"

    On Ubuntu 24.04, install the compiler, Qt development files and platform
    libraries:

    ```sh
    sudo apt-get update
    sudo apt-get install -y build-essential clang cmake ninja-build pkg-config \
      nasm qt6-base-dev qt6-base-private-dev qt6-tools-dev qt6-svg-dev \
      libpulse-dev libasound2-dev libvulkan-dev libgl1-mesa-dev libx11-dev \
      libxext-dev libxi-dev libxrandr-dev libxrender-dev xvfb patchelf
    python3 scripts/build_emulator.py all \
      --workspace .symbian/emulator-build --qt-prefix /usr
    ```

    Run in a desktop session. For headless startup checks, use Xvfb with Mesa
    software rendering. Both x86_64 and aarch64 use this native build recipe.

For a separately installed Qt 6.8.3, pass its installation prefix through
`--qt-prefix`. Use `--jobs N` to control build parallelism. Stages `acquire`,
`ffmpeg`, `sdl`, `configure`, `build` and `test` can also be run separately.
The driver creates CMake/Ninja presets and a compilation database in the
workspace. The frontend disables camera and Lua integration.

The release source archive also includes patched sources and configured
Abseil, JSON and libuv sources. Its `BUILD.md` shows how to rebuild Qt and use
`--source-tree` and `--dependency-sources` without downloading those inputs.
The per-host runtime source archives contain the corresponding library sources
and package rebuild recipes.

## Isolated instance

The patched frontend accepts an explicit data root. Use a fresh root for each
run and do not launch the preserved firmware baseline directly:

=== "macOS"

    ```sh
    export SYMBIAN_EMULATOR="$PWD/.symbian/emulator-build/build/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
    EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
    ```

=== "Linux"

    The pinned CMake target sets its executable output to `bin/eka2l1_qt`:

    ```sh
    export SYMBIAN_EMULATOR="$PWD/.symbian/emulator-build/build/bin/eka2l1_qt"
    export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
    EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
    ```

The root holds `config.yml`, device data, writable drives and logs. On macOS, a direct manual launch may become the foreground app; the SDK
Run supervisor uses an accessory window in its maintained EKA2L1 patch. The
window can remain visible over a full-screen terminal, accept dragging, and
leave that terminal frontmost. The supervisor owns and cleans up its disposable process. Return to
[Run the GUI example](../guides/gui-emulator.md) for application launch.
