# Host native builds and wheels

Build/test layout follows A11: an isolated dependency prefix, native CMake/Ninja
presets and GTest, a separate Python wheel build, and an installed-wheel audit.
Native libraries and tests default to exceptions disabled. The host
Boost.Fiber implementation enables them in three translation units required
for Boost.Context unwinding; pybind11 boundary files are the other explicit
exception scope. Linux hides statically linked vendor symbols.

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

The prefix supplies static OpenSSL, libusb and Boost archives. The bootstrap
is adapted from A11's source/hash/cache approach and pins OpenSSL 3.5.9 with
its official archive digest. Static linkage is enforced on Linux and macOS. Linux checks
Perl prerequisite modules, including IPC::Cmd and Time::Piece. Abseil,
nlohmann::json and pybind11_abseil have pinned source revisions/hashes; optional
source-directory overrides support offline prepared inputs.

`build/debug/compile_commands.json` includes the prepared GUI's actual ARM
commands when Clang and source SDK headers are available. Host libraries retain
native commands. `gui_app` links ARM ELF; `gui_app_e32` runs the independent
publisher; `gui_app_run` is a native executable suitable for CLion Run. See
the [CLion guide](clion.md). The [guest runtime](../capabilities/runtime.md)
is a separate target configuration.

## Linux evidence and release gates

[Linux CI run 37202283905](https://github.com/hpnkv/symbian-platform/actions/runs/37202283905)
passed these host checks on Ubuntu 24.04 and manylinux_2_28:

| Host architecture | Native CTest | Installed wheel audit | Console GUI dependency |
| --- | --- | --- | --- |
| x86_64 | 10/10 | Python 3.11–3.14 | PySide6 on 3.11–3.13; 3.14 CLI only |
| aarch64 | 10/10 | Python 3.11–3.14 | CLI only |

Each wheel was built, repaired by auditwheel, installed in a fresh environment
and checked for native imports, CLI doctor, packaged resources and ELF loader
dependencies. An earlier native Linux aarch64 CPython 3.12 experiment also
passed 62 installed-wheel status/HTTP/parser/CLI/preservation/control Pytest
cases. These checks establish a host core and installable wheels. The Linux
Qt frontend, source SDK export, ARM guest launch, GDB/CLion, display/input
and real firmware fixtures remain separate gates.

`.github/workflows/host-linux.yml` separates native and cibuildwheel gates for
x86_64/aarch64. Cibuildwheel covers CPython 3.11–3.14 and manylinux_2_28.
The macOS baseline remains arm64/macOS 26.0 based on the actual local native
closure, rather than retagging a binary for older systems.

Audit an installed wheel from outside the checkout:

```sh
python -m venv /tmp/symbian-wheel-check
/tmp/symbian-wheel-check/bin/pip install /absolute/path/to/the.whl
cd /tmp
/tmp/symbian-wheel-check/bin/python -m symbian.build_support.audit \
  /absolute/path/to/the.whl
```

The audit contains A11's actual Mach-O/ELF closure checks and exercises native
parsers, structured Status details, Pydantic, the canonical caster modules,
CLI doctor and packaged resources. Linux release wheels also need cibuildwheel's
repair/tagging gate; the directly built experimental linux_aarch64 wheel is not
itself a published manylinux wheel. No packages have been uploaded.

The one-install compiler/header/emulator/debugger payload design is in the
[distribution plan](https://github.com/hpnkv/symbian-platform/blob/main/.dev/distribution.md).
Those payloads are not yet included in the host tooling wheel.
