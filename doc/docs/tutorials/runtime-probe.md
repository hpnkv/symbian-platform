# C++ runtime probe

The example uses LLVM libc++ strings and vectors with repeated allocation and
destruction. It also includes separate allocation-failure controls.
Use the [guest runtime guide](../capabilities/runtime.md) to select the matching
headers and archive, and [source preparation](../guides/source-prerequisites.md)
for external dependencies.

From a prepared source checkout:

```sh
uv run symbian build --project examples/runtime_probe \
  --output .symbian/runtime-probe
uv run symbian inspect --format e32 .symbian/runtime-probe/runtime_probe.exe
```

Keep the matching ELF for debugging. Run with compatible firmware in a
[disposable emulator instance](../guides/gui-emulator.md) and inspect the native
exit reason. The runtime supports writable data/BSS, local GOT fixups, global
initialization and compiler arithmetic helpers; general C++ TLS and thread-safe
local-static guards are unsupported.
