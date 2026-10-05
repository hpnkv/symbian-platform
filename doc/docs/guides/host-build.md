# Build the host SDK and Python wheel

Use CMake/Ninja presets for native builds, GTest for native tests and Pytest
for Python and integration tests. Install prerequisites from the
[macOS and Linux source guide](source-prerequisites.md).

## Build and test from a checkout

```sh
export SYMBIAN_DEPS_PREFIX="$PWD/.symbian/host-deps"
scripts/bootstrap_wheel_deps.sh
cmake --preset debug
cmake --build --preset debug --parallel 4
ctest --preset debug
uv sync
uv run pytest -q
uv build --wheel
```

The isolated prefix supplies static OpenSSL, libusb, Boost and zlib.
Abseil, nlohmann::json and pybind11_abseil use pinned source revisions;
source-directory overrides allow prepared offline inputs. On Linux the
bootstrap also needs Perl's `IPC::Cmd` and `Time::Piece` modules.

Native libraries default to exceptions disabled. Selected Boost implementation
and pybind11 translation units explicitly enable exceptions where required.
Linux hides statically linked vendor symbols.

`build/debug/compile_commands.json` contains host commands and, when the source
SDK is prepared, the configured GUI's ARM commands. Host and guest builds use
separate target configurations. See [CLion setup](clion.md) and the
[guest runtime](../capabilities/runtime.md).

## Check an installed wheel

Install a built wheel into a fresh environment and run the audit outside the
checkout:

```sh
python -m venv /tmp/symbian-wheel-check
/tmp/symbian-wheel-check/bin/pip install /absolute/path/to/the.whl
cd /tmp
/tmp/symbian-wheel-check/bin/python -m symbian.build_support.audit \
  /absolute/path/to/the.whl
```

The audit checks Mach-O/ELF dependency closure, native imports, structured
statuses, CLI tools and packaged resources. Linux distribution wheels also
require manylinux repair and tagging; a local `linux_x86_64` or
`linux_aarch64` wheel is not automatically a manylinux wheel.

The host tooling wheel does not include firmware. Prepare the native target
SDK separately using [SDK installation](../reference/project-configuration.md).
