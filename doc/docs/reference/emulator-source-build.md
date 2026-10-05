# Build the emulator from source

The application guides use a prepared EKA2L1 installation. Build the pinned
research emulator only when investigating emulator behavior, guest debugging
or a missing system service. EKA2L1 is an external GPL project; keep its
checkout and build products outside version control and preserve its licenses.

## Source and patches

Use EKA2L1 commit `2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8` and its
pinned submodules. From the platform repository root, create a fresh checkout
and apply the maintained patches in order:

```sh
mkdir -p research/upstream
git clone --no-checkout https://github.com/EKA2L1/EKA2L1 research/upstream/EKA2L1
git -C research/upstream/EKA2L1 checkout 2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8
git -C research/upstream/EKA2L1 submodule update --init --recursive --depth 1
for patch in \
  instance-root runtime-probe guest-debug-step guest-debug-library-query \
  symbian101-experimental guest-thread-register guest-control \
  firmware-import-bounds fbs-unsupported-request background-window \
  dll-wsd-dyncom-exit belle-library-entry-start belle-library-load-prepare \
  belle-thread-exit-reason dyncom-strexd-value v10-thread-exit-reason \
  ntick-fast-counter-hal fast-counter-rate belle-recv-from-no-length \
  belle-nonblocking-tcp belle-secure-random belle-recv-one-or-more-no-length
do
  git -C research/upstream/EKA2L1 apply "$PWD/research/eka2l1/$patch.patch"
done
```

The patches add isolated instance roots, firmware import checks, loopback guest
debugging, capture/input controls and service compatibility fixes. Inspect an
existing checkout before applying them; do not reapply patches already present.

The frontend needs Qt and an FFmpeg build compatible with the host.
The pinned source tree contains `linux_x64-build.sh` for x86_64; it has no
maintained Linux aarch64 FFmpeg script here. Use the same pinned checkout and
local patch sequence on either host. The configured build also creates
historical checksum and validator executables as separate oracles; their
algorithms are not linked into the SDK's native format implementation.

=== "macOS"

    On Apple Silicon, use Homebrew Qt and the pinned ARM64 FFmpeg
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

=== "Linux x86_64"

    The frontend builds on Ubuntu 24.04 x86_64 with Clang 20. Install the
    build dependencies (package names below are for Ubuntu):

    ```sh
    sudo apt-get update
    sudo apt-get install -y build-essential clang-20 lld-20 cmake ninja-build \
      pkg-config nasm qt6-base-dev qt6-base-private-dev qt6-tools-dev \
      qt6-svg-dev qt6-multimedia-dev libsdl2-dev libpulse-dev libasound2-dev \
      libvulkan-dev libgl1-mesa-dev libx11-dev xvfb gdb-multiarch
    ```

    After the ordered root patches, apply the two submodule compatibility
    patches. They constrain x86 shift immediates, disable old FFmpeg's
    incompatible Vulkan video acceleration, and specialize Dynarmic's
    integer-sequence template for contemporary libstdc++. The emulator's
    graphics backend remains available.

    ```sh
    git -C research/upstream/EKA2L1/src/external/ffmpeg apply \
      "$PWD/research/eka2l1/ffmpeg-linux-compat.patch"
    git -C research/upstream/EKA2L1/src/external/dynarmic/externals/mcl apply \
      "$PWD/research/eka2l1/mcl-integer-sequence.patch"
    (cd research/upstream/EKA2L1/src/external/ffmpeg && \
      MAKEFLAGS=-j8 sh linux_x64-build.sh)
    cmake -S research/upstream/EKA2L1 -B build/eka2l1 -G Ninja \
      -DCMAKE_BUILD_TYPE=RelWithDebInfo \
      -DCMAKE_C_COMPILER=clang-20 -DCMAKE_CXX_COMPILER=clang++-20 \
      -DQT_DIR=/usr/lib/x86_64-linux-gnu/cmake/Qt6 \
      -DQT_DEFAULT_MAJOR_VERSION=6 \
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

=== "Linux"

    The pinned CMake target sets its executable output to `bin/eka2l1_qt`:

    ```sh
    export SYMBIAN_EMULATOR="$PWD/build/eka2l1/bin/eka2l1_qt"
    export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
    EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
    ```

The root holds `config.yml`, device data, writable drives and logs. On macOS, a direct manual launch may become the foreground app; the SDK
Run supervisor uses an accessory window in its maintained EKA2L1 patch. The
window can remain visible over a full-screen terminal, accept dragging, and
leave that terminal frontmost. The supervisor owns and cleans up its disposable process. Return to
[Run the GUI example](../guides/gui-emulator.md) for application launch.
