# Research emulator build

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

The macOS research build needs Qt and FFmpeg compatible with the host
architecture. It uses CMake and Ninja. The configured build also creates
historical checksum and validator executables as separate oracles; their
algorithms are not linked into the SDK's native format implementation.

```sh
cmake -S research/upstream/EKA2L1 -B build/eka2l1 -G Ninja \
  -DCMAKE_BUILD_TYPE=RelWithDebInfo \
  -DCMAKE_EXPORT_COMPILE_COMMANDS=ON \
  -DCMAKE_PREFIX_PATH="$(brew --prefix)" \
  -DCMAKE_PROJECT_EKA2L1_INCLUDE="$PWD/research/eka2l1/project-tests.cmake"
cmake --build build/eka2l1 -j 8 --target eka2l1_qt
```

## Isolated instance

The patched frontend accepts an explicit data root. Use a fresh root for each
run and do not launch the preserved firmware baseline directly:

```sh
export SYMBIAN_EMULATOR="$PWD/build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
export SYMBIAN_GUI_INSTANCE="$PWD/.symbian/instances/gui-development"
EKA2L1_DATA_ROOT="$SYMBIAN_GUI_INSTANCE" "$SYMBIAN_EMULATOR" --help
```

The root holds `config.yml`, device data, writable drives and logs. A direct
manual launch may become the foreground macOS app. The SDK Run supervisor
uses its own background-window policy and disposable state. Return to
[Run the GUI example](../guides/gui-emulator.md) for the application task.

The [development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
records which firmware and CPU backend controls passed. Emulator execution
cannot establish Nokia 808 compatibility.
