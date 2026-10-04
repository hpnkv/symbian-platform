# Packaging E32 applications as SISX

The native writer packages one validated E32 executable, including eager imports. The output
is an unsigned SISX-format `.sis` file for ordinary installation, with separate
package and executable UIDs. It currently supports English, printable ASCII
metadata, a restricted executable basename, versions in 0..32767, one payload
up to 16 MiB, and installation to `!:\sys\bin\<name>.exe`. It has no resources,
scripts, dependencies, embedded packages, upgrades or certificates.

This advances the build/package/emulator loop. It does not establish
installation policy or loader compatibility on the physical Nokia 808.
A supplied Delight RM-807 firmware bundle now imports into EKA2L1; GUI startup
and visual behavior in that material remain under investigation.

The host build requires OpenSSL 3's static libcrypto for the legacy SHA-1 field.
The isolated dependency bootstrap in the [host build guide](host-build.md)
supplies it on macOS or Linux. It follows A11's static linkage pattern; the
verified wheel does not depend on a package-manager crypto library and includes
the OpenSSL license.
No historical SDK or Windows utility is needed for this profile. Native SIS
code returns Abseil statuses, compiles without exceptions and is synchronous.
Bindings release the GIL for native work; Python handles files, TOML and reports.

Add a package table to symbian.toml, independently of the project table:

```toml
[package]
uid = 0xe0000809
name = "Symbian E32 Probe"
vendor = "Symbian research"
executable_name = "probe.exe"
version = [1, 0, 0]
```

Package fields are required and unknown fields are rejected. Experimental UIDs
must be in 0xe0000000..0xefffffff. Package UID 0xe0000809 differs from the probe's
executable UID/SID 0xe0000808. The binary format, checksums and digest remain in
C++; there is no `.pkg` interpreter or Python binary-format implementation.

```sh
uv run symbian build --project examples/e32_probe --output .symbian/e32-probe
uv run symbian package --project examples/e32_probe \
  --artifact .symbian/e32-probe/e32_probe.exe --output .symbian/package
uv run symbian inspect --format sis .symbian/package/probe.sis
```

The writer uses uncompressed streams and the fixed historical 2004-01-01 epoch,
and repeats generation before publishing. package-report.json records input,
manifest and package SHA-256 digests. SHA-1 is a legacy integrity field, not
package authentication. The inspector supports precisely this canonical profile:
it bounds containers, verifies UID and controller/data CRCs and the file digest,
validates the E32 through the native core, then checks a canonical reconstruction.
Native inspection exposes the verified embedded executable SHA-1. This permits
independent pairing with an exact supplied executable before installer work.
Other SIS profiles return Unimplemented, not a general validity verdict.

After building the research dependencies in EKA2L1 research notes:

```sh
uv run symbian toolchain verify-package .symbian/package/probe.sis \
  --executable .symbian/e32-probe/e32_probe.exe \
  --output .symbian/package-check
```

This is specific to the maintained probe and package metadata. It runs 35 GTest
cases: the existing 28 image/CPU/kernel checks, one independent Nokia checksum
case and six EKA2L1 package cases. Each package case has a fresh private filesystem,
registry and kernel. The unchanged installer writes the executable, whose bytes
must match the independently supplied E32. Both CPU backends launch it through
the real emulator kernel. Further cases reload the registry, uninstall, confirm
missing-process rejection, reinstall and execute after a prior failure exit.
The package manager also exposes the file's UID/SID, version and SHA-1 digest.
The digest is compared to an independently recorded hashlib value for this probe.
No Belle ROM, target DLL or GUI application runs.

Reports preserve private input copies, full test JSON, logs and binary hashes.
Filters, shards, skips or failures cannot silently produce success. Installation,
registry reload and reinstall evidence are reported separately from Belle runtime
and phone installation flags, which remain false. EKA2L1's current installer
stores the digest but does not enforce phone signing/capability policy; the native
inspector is therefore required before this trusted-fixture experiment. The
research harness is not a general API for installing untrusted SIS files.

Golden artifact on the verified Apple Silicon toolchain:

- E32 SHA-256: `997cd9c5ec35281f261a08cb3c2ca6a36c74be969b4a72cbbd8ede5ff5332395`
- SIS, 908 bytes, SHA-256: `9065031847f1db3b1fae1779b478208c38ea3f61a2e5554c2bd293106903abb9`

See .dev/research-log.md for pinned sources and experiment details. Hardware
preservation and recovery remain gates for physical-device development.


The pointer/virtual dispatch example also produces a package through this
unchanged native writer. `toolchain verify-pointers --package` runs the scoped
21-case image/dispatch/installer check. See POINTERS.md. The original
`verify-package` command retains the earlier relocation-free probe scope.

The GUI example now has package UID 0xe0000812 and executable UID 0xe0000811.
Its imported E32 remains opaque unchanged payload; EUSER/WS32 must already exist
in the target. DLL payloads remain unsupported. Build it and check its package:

```sh
uv run symbian package --project examples/gui_app \
  --artifact .symbian/gui-app/gui_app.exe --output .symbian/gui-package
uv run symbian toolchain verify-gui-package .symbian/gui-package/gui_app.sis \
  --executable .symbian/gui-app/gui_app.exe --output .symbian/gui-package-check
```

Build symbian_gui_package_probe alongside the existing research oracles. This
17-case check combines eight image checks, one SIS CRC oracle and eight GUI
package cases. The original installer checks unchanged bytes, complete metadata,
SID and an independent hashlib reference; registry reload, uninstall and
reinstall are checked in fresh private filesystems with both CPU backends
configured. No CPU instructions execute. A fourth case observes that the
unchanged upstream loader creates a process despite absent system DLLs, leaving
all 37 import slots unresolved. The report explicitly preserves that defect
and keeps GUI/SDK execution, Belle, debugging and phone flags false.
Equal-sized different valid executables are rejected before installer work by
the independent digest pairing check. The installed wheel reproduces ELF/E32/SIS
and repeats all 17 cases. See source walkthrough and .dev/research-log.md.

The GUI package is 7,036 bytes, SHA-256
`7c7f2a402c4f22175c2e120fdc6dd39f37508168908e28fe07ee50828b6f4ebf`.
