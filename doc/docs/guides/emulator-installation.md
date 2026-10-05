# Install the Symbian emulator

The SDK uses a separately distributed [EKA2L1](https://github.com/EKA2L1/EKA2L1)
frontend with its Qt desktop libraries, plugins and firmware importer. The
emulator is GPL-3.0-or-later; it runs as a separate executable. Firmware and
guest libraries, including Symbian Qt, are supplied separately.

The installation commands below require SDK 0.1.2 or later.

With the SDK's Python commands installed, select a compatible native bundle:

```sh
symbian emulator install
symbian emulator doctor
symbian emulator list
```

The installer chooses your macOS or Linux architecture and checks the actual
executable's control protocol and features before selecting it. Emulator
versions have their own release cadence. An SDK upgrade keeps a compatible
emulator; SDK and emulator version numbers need not match.

For an offline installation, download the matching archive from the
[emulator releases](https://github.com/hpnkv/symbian-platform/releases?q=emulator-v):

```sh
symbian emulator install --archive '/path/to/symbian-emulator-bundle.tar.gz'
```

Add `--sha256 HEX_DIGEST` to verify a digest obtained separately. Explicit
versions let you upgrade or return to a retained installation:

```sh
symbian emulator install --version 0.1.0
symbian emulator select 0.1.0
```

Installation uses the per-user XDG data directory, normally
`~/.local/share/symbian/emulators`. Selection uses the XDG configuration
directory. Old versions remain available for rollback. Installation does not
replace firmware or emulator sessions. Explicit project, SDK or command tool
paths take precedence; `symbian emu resolve` shows the effective selection.

On Linux, use glibc 2.39 or later, a desktop session with X11 or Wayland and working graphics
drivers. The bundle supplies Qt and its non-system dependencies; the system
supplies glibc and graphics-driver interfaces. macOS bundles require macOS 15
or later. Initial macOS bundles use
ad hoc signatures. Gatekeeper may require approval to open downloaded code;
use the normal macOS security settings after checking its origin.

Next, [import local firmware and run an application](firmware.md). For a
custom frontend, use the [source build reference](../reference/emulator-source-build.md)
and select its executable through `symbian emu configure --scope global
--emulator /path/to/frontend --importer /path/to/symbian_firmware_tool`.
