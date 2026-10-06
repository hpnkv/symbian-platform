# Create a standalone application

A standalone project keeps its C++ source, build settings and icon together. It
uses an installed Symbian SDK to make an ARM application; you can build without
firmware and add an emulator device when you are ready to run it.

## 1. Create the project

[Download the native archive for your host](native-distributions.md), then
install it in your activated Python environment:

```sh
symbian sdk install ~/dev/symbian-sdk --archive symbian-sdk.tar.gz
```

For a source checkout with the upstream inputs already prepared, use
`--workspace ~/dev/symbian` instead of `--archive`.

Then create an application:

```sh
symbian init ~/dev/hello_time --name hello_time --non-interactive
```

`init` creates the starter source and builds it once. ARMv6 is the default. Use
`--architecture armv5t` if your target requires ARMv5T. A nonempty destination
is refused. The starter displays a clock and responds to taps and Clear/Exit.

## 2. Find the parts you will edit

| File | Purpose |
| --- | --- |
| `app.cc` | Local state, clock formatting, drawing, input and timer tasks |
| `symbian.toml` | App identity, menu captions and icon |
| `sdk-location.json` | Local SDK selection; ignored by Git |

Window Server is the Symbian service that owns GUI windows and delivers redraw
and input events. The SDK's startup bridge handles the lower-level entry path.
The starter always enables the modern C++ runtime, Abseil Status and timer
tasks. Its selected firmware must provide `libpthread.dll`.
For a tour of that path, see the [GUI example](from-source.md).

## 3. Build and run

```sh
symbian app build --project ~/dev/hello_time
symbian app run --project ~/dev/hello_time
```

A run needs a separately supplied firmware image imported into the local
[firmware store](firmware.md). The emulator launches a disposable copy, so the
stored baseline remains available for the next run.

## Where to go next

- [Build and inspect an E32 application](building.md) explains the generated
  files and build checks.
- [Configure a local firmware image](firmware.md) gets the app into EKA2L1.
- [Use CLion](clion.md) walks through CMake profiles, Run and Debug.
- [SDK selection and project configuration](../reference/project-configuration.md)
  covers relocation, precedence, localization and installed files.
- [Native library targets](../reference/project-libraries.md) covers static
  libraries, DLLs and application links.
