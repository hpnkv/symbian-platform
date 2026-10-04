# Create a standalone application

A standalone project keeps its C++ source, build settings and icon together. It
uses an installed Symbian SDK to make an ARM application; you can build without
firmware and add an emulator device when you are ready to run it.

## 1. Create the project

Install the active SDK first if you have not done so:

```sh
symbian sdk install ~/dev/symbian-sdk --workspace ~/dev/symbian
```

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
| `model.h`, `model.cc` | Application behavior and state |
| `app.cc` | Window Server drawing and input |
| `symbian.toml` | App identity, menu captions and icon |
| `sdk-location.json` | Local SDK selection; ignored by Git |

Window Server is the Symbian service that owns GUI windows and delivers redraw
and input events. The SDK's startup bridge handles the lower-level entry path.
For a tour of that path, see the [GUI example](from-source.md).

## 3. Build and run

```sh
symbian app build --project ~/dev/hello_time
symbian app run --project ~/dev/hello_time
```

A run needs a separately supplied firmware image imported into the local
[firmware store](firmware.md). The emulator launches a disposable copy, so the
stored baseline remains available for the next run. An emulator result is
useful development evidence; it does not establish compatibility with a
physical Nokia 808.

## Where to go next

- [Build and inspect an E32 application](building.md) explains the generated
  files and build checks.
- [Configure a local firmware image](firmware.md) gets the app into EKA2L1.
- [Use CLion](clion.md) walks through CMake profiles, Run and Debug.
- [SDK selection and project configuration](../reference/project-configuration.md)
  covers relocation, precedence, localization and installed files.
- [Native library targets](../reference/project-libraries.md) covers static
  libraries, DLLs and application links.
