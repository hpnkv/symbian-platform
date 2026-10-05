# Investigate the guest C++ runtime

The SDK supplies a C++ runtime for code that executes inside Symbian. The
project chooses a target architecture and links only the library profiles it
uses. Guest code has exceptions disabled by default; application and library
errors use explicit status results.

## Choose one question

| Goal | Start with |
| --- | --- |
| Use strings, containers or clocks | [C++ capabilities](../capabilities/cpp.md) |
| Add a timer or bounded asynchronous task | [Concurrency](../capabilities/concurrency.md) |
| Link a native library or DLL | [Library targets](../reference/project-libraries.md) |
| Explore startup, imports and ABI behavior | [Runtime reference](../reference/guest-runtime-research.md) |

The [runtime capability page](../capabilities/runtime.md) describes the library
profiles and restrictions. Run the smallest relevant example before adding a
feature to your application.

## Build and inspect a small runtime program

From a prepared source checkout, build the string/vector example with the
selected SDK:

```sh
symbian build --project examples/runtime_probe --output .symbian/runtime-probe
symbian inspect --format e32 .symbian/runtime-probe/runtime_probe.exe
```

Keep the matching ELF for debugging. Run the executable in a
[disposable emulator instance](gui-emulator.md) with compatible firmware and
inspect its native exit reason. The example exercises repeated allocation and
destruction and includes separate allocation-failure controls.
