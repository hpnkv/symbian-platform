# Build and inspect an application

A Symbian application build produces an ARM **ELF** with debug symbols and an
**E32** image that the Symbian loader reads. CMake and Ninja compile the source;
the SDK's converter checks the linked ELF and writes E32. A **SIS** installer is
a separate packaging result.

## 1. Build the starter

After [creating a project](projects.md), run:

```sh
symbian app build --project ~/dev/hello_time
```

The project selects the installed SDK and architecture. Build output and logs
stay in its ignored `.symbian/` directory. A missing import, unsupported
relocation or incompatible target setting fails the build instead of creating
an apparently usable image.

## 2. Inspect the output

Use the E32 path reported by the build:

```sh
symbian inspect --format e32 /path/to/hello_time.exe
```

Inspect reports image metadata; it does not execute the application. Keep the
ELF as well: its DWARF symbols let ARM GDB map guest addresses back to your
source. The [GUI source walkthrough](gui-build.md) shows concrete ELF and E32
paths for the maintained example.

## 3. Check it in an emulator

Import a local ROM/Z using the [firmware guide](firmware.md), then run:

```sh
symbian app run --project ~/dev/hello_time
```

An ARM ELF or converted E32 passing structural checks does not prove that the
Symbian loader accepts it. Run the application in the emulator and check its
native exit report; test installation and required services on the target device
before deployment.

For a custom source graph, linker script, reproducibility report or low-level
PIC probe, continue to the [E32 build pipeline](../reference/e32-build-pipeline.md).
