# Build and inspect the GUI executable

```sh
uv run symbian build --project examples/gui_app --output .symbian/gui-app
uv run symbian inspect .symbian/gui-app/gui_app.elf --format elf32
uv run symbian inspect .symbian/gui-app/gui_app.exe --format e32
```

The CLI loads `symbian.toml`, resolves the selected SDK's proxy paths and
passes the target toolchain to the project's preset. The project builds with Ninja, converts with the native
E32 writer, and repeats the build in a separate directory. It compares the
ELF and E32 bytes and records actual dependencies. No Python implementation
of ELF/E32/DEF parsing is used.

| Output | Use |
| --- | --- |
| `.symbian/gui-app/gui_app.exe` | Guest E32 executable |
| `.symbian/gui-app/gui_app.elf` | Original ARM ELF with symbols and DWARF |
| `.symbian/gui-app/report.json` | Hashes, inputs, tool versions, build logs and native metadata |
| `.symbian/gui-app/compile_commands.json` | Real CMake compilation database for editors |
| `.symbian/gui-app/cmake/` | Persistent CMake/Ninja target tree |

Edit `app.cc`, `window_server.cc` or `model.h` and rerun `symbian build`. The persistent tree provides
the incremental build; the second build rechecks reproducibility. You can also
use `cmake --build .symbian/gui-app/cmake` for quick compiler feedback, but that
alone does not reconvert or update the published `.exe` and report.

Changing the SDK path requires both a `SYMBIAN_GUI_SDK_INCLUDE` CMake cache
override and updated `import_proxies` in the project manifest. An ignored
`CMakeUserPresets.json` can inherit `symbian-pic`; set `cmake_preset` to its
name. The integration fixture in `symbian/tests/test_gui.py` demonstrates this
with paths containing spaces. Do not copy an arbitrary SDK onto the source
profile and assume the frozen ordinals remain valid.

The native package writer accepts imported executables and, when the manifest
has `[application]`, bundles genuine compiled registration/caption resources
on the executable's install drive. It still rejects DLL payloads. The project
declares package UID
`0xe0000812`, independently of executable UID `0xe0000811`:

```sh
uv run symbian package --project examples/gui_app \
  --artifact .symbian/gui-app/gui_app.exe --output .symbian/gui-package
uv run symbian inspect .symbian/gui-package/gui_app.sis --format sis
uv run symbian toolchain verify-gui-package .symbian/gui-package/gui_app.sis \
  --executable .symbian/gui-app/gui_app.exe \
  --oracles-build build/eka2l1 --output .symbian/gui-package-check
```

Edit menu text in `examples/gui_app/symbian.toml`:

```toml
[application]
caption = "Symbian GUI Counter"
short_caption = "Counter"
icon = "assets/icon.svg"

[application.localizations.de]
caption = "Symbian Zähler"
short_caption = "Zähler"
```

The package contains the unchanged EXE, fallback and translated menu
resources, and an SDK-compiled SVG-in-MIF icon on the same install drive;
target system libraries must already exist. Native inspection checks every
embedded hash, resource UID and install path. The original EKA2L1 AppArc
parser selects the French, German and Japanese resources, decodes `Zähler`
and `カウンター` as Unicode,
and its MIF reader extracts the SVG icon. Its installer accepted the seven
files on Dyncom and Dynarmic, reloaded
their registry entry, removed the resources on uninstall and reinstalled them
(eight headless tests). The full verifier passed 17 original image/checksum
and installer cases through the visible SDK. These cases execute no guest
instructions. The current GUI source has six DLL imports and 153 import
slots. Physical Belle icon rendering remains to be observed. The emulator's
process creation without system DLLs leaves those slots
unresolved, which is not an application
launch. Certificates and physical-phone installation remain separate gates.

# 5. Run checks that do not require a ROM

Build and run the host GTests and Python policy tests:

```sh
cmake --preset debug
cmake --build --preset debug -j 8
ctest --preset debug
SYMBIAN_GUI_SOURCE_ROOT="$PWD/research/upstream" uv run pytest \
  symbian/tests/test_gui.py -q
```

The source environment variable enables the real SDK/link fixture. Without it,
the two source-dependent cases skip; they are not silently counted as passing.
To enable historical validation also build the research oracles and supply their
path:

```sh
SYMBIAN_GUI_SOURCE_ROOT="$PWD/research/upstream" \
  SYMBIAN_EKA2L1_ORACLES_BUILD="$PWD/build/eka2l1" \
  uv run pytest symbian/tests/test_gui.py -q
uv run symbian toolchain verify-gui .symbian/gui-app/gui_app.exe \
  --oracles-build build/eka2l1 --output .symbian/gui-validation
```

The research build recipe below supplies the oracle executables.
`verify-gui` runs one checksum case and seven unchanged historical validator
cases against a private exact copy of this generated image. It retains binary
hashes, test JSON, logs and `report.json`. It does not execute GUI instructions,
resolve target system DLLs, or boot an OS. `verify-probe` and `verify-pointers`
are specific to other maintained examples; they cannot substitute for a GUI
execution test. All GUI runtime, import, loader and debugger flags stay false.

For formatting and the full Python suite:

On macOS, use `$(brew --prefix llvm)/bin/clang-format` in place of
`clang-format` if Homebrew's LLVM bin directory is not on `PATH`.

```sh
uv run black --check symbian scripts
uv run ruff check symbian scripts
clang-format --dry-run --Werror \
  examples/gui_app/app.cc examples/gui_app/window_server.cc \
  examples/gui_app/startup.cc \
  examples/gui_app/model.h cpp/tests/gui_model_test.cc
uv run pytest -q
```

The full suite also has optional emulator/header/module inputs described in
[EKA2L1 research notes](https://github.com/hpnkv/symbian-platform/blob/main/.dev/research/eka2l1.md) and
[C++20 guide](../capabilities/cpp20.md). Missing optional inputs must remain visible as
skips. None of these tests issues a hardware operation.
