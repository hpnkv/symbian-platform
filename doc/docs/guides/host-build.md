# Host native builds and wheels

Build/test layout follows A11: an isolated dependency prefix, native CMake/Ninja
presets and GTest, a separate Python wheel build, and an installed-wheel audit.
All native libraries/tests compile without exceptions; only pybind11 boundary
translation units enable them. Linux hides statically linked vendor symbols.

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

The prefix supplies static OpenSSL libcrypto. The bootstrap is adapted from
A11's actual source/hash/cache approach and pins OpenSSL 3.5.9 with its official
archive digest. Static linkage is enforced on Linux and macOS. Linux checks
Perl prerequisite modules, including IPC::Cmd and Time::Piece. Abseil,
nlohmann::json and pybind11_abseil have pinned source revisions/hashes; optional
source-directory overrides support offline prepared inputs.

`build/debug/compile_commands.json` includes the prepared GUI's actual ARM
commands when Clang and source SDK headers are available. Host libraries retain
native commands. `gui_app` links ARM ELF; `gui_app_e32` runs the independent
publisher; `gui_app_run` is a native executable suitable for CLion Run. See
CLION.md. The guest runtime is a separate target configuration (RUNTIME.md).

## Linux evidence and release gates

A native Linux aarch64 manylinux_2_28 container, CPython 3.12, built all seven
host CTest targets and a real wheel. The installed wheel passed the loader
closure audit and 62 status/HTTP/parser/CLI/preservation/control Pytest cases.
Tests were copied outside the source package to ensure imports used the installed
wheel. The initial source-path run exposed shadowing, which was corrected.

The Linux wheel contains all three native extension modules, uses static
OpenSSL and passes the ELF system-library/RPATH allowlist. This establishes the
host core on Linux; Linux Qt frontend, ARM guest launch, GDB/CLion, display/input
and actual firmware fixtures remain separate gates. x86_64 and other CPython
builds are configured in CI but have not been executed locally.

`.github/workflows/host-linux.yml` separates native and cibuildwheel gates for
x86_64/aarch64. Cibuildwheel is configured for CPython 3.11–3.14 and manylinux_2_28.
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

The one-install compiler/header/emulator/debugger payload design is in
DISTRIBUTION.md. Those payloads are not yet included in the host tooling wheel.
