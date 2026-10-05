# Package an application as SIS

The SDK's native writer packages an EKA2 E32 executable into a SISX-format
`.sis` file. Projects may include compiled application registration, localized
menu captions, an SVG-in-MIF icon and a project-local CA bundle. The default
package is unsigned; [signing](signing.md) adds an RSA/X.509 signature.
Target system DLLs must already exist on the device.

## Define package identity

Add a package table to `symbian.toml`, independently of the executable identity:

```toml
[package]
uid = 0xe0000809
name = "Symbian E32 Probe"
vendor = "Example developer"
executable_name = "probe.exe"
version = [1, 0, 0]
```

All package fields are required and unknown fields are rejected. The supported
profile uses experimental UIDs in `0xe0000000..0xefffffff`; package and executable
UIDs are distinct. Use separate identities for a released application.

## Build, package and inspect

For the small diagnostic E32 probe, from a source checkout:

```sh
symbian build --project probes/e32_probe --output .symbian/e32-probe
symbian package --project probes/e32_probe \
  --artifact .symbian/e32-probe/e32_probe.exe --output .symbian/package
symbian inspect --format sis .symbian/package/probe.sis
```

The writer validates the E32 payload, uses uncompressed streams and a fixed
historical timestamp, and compares repeated generation before publication.
`package-report.json` records input and output digests. The native inspector
checks UID and controller/data CRCs, embedded-file hashes and E32 metadata.
Unsupported SIS profiles return `Unimplemented`; the inspector is not a
universal historical SIS validator. SHA-1 is a legacy integrity field, not
package authentication.

## Add application resources

Generated GUI projects include an `[application]` table. Edit captions,
localizations and the project-relative SVG icon there; the SDK compiles the
resources with `rcomp` and packages them on the executable's install drive.
See [project configuration](../reference/project-configuration.md).
Projects without this table remain executable-only packages and may have no
application-menu entry.

For the GUI example:

```sh
symbian package --project examples/gui_app \
  --artifact .symbian/gui-app/gui_app.exe --output .symbian/gui-package
symbian inspect --format sis .symbian/gui-package/gui_app.sis
```

Use the [TLS guide](tls.md) to package a PEM CA bundle. Packaging roots does
not change the device-wide trust store or automatically load them into a TLS
session.

## Restrictions and installation

Package metadata is printable ASCII with versions from 0 to 32767. Executable
payloads are limited to 16 MiB and install to `!:\sys\bin\<name>.exe`.
DLL payloads, arbitrary installer scripts, dependency declarations, embedded
packages, upgrades and legacy EKA1 SIS are unsupported.

[Stage a package](device.md) through a mounted volume or MTP, then complete
installation on the handset. A signature does not grant capabilities; the
phone's certificate and installation policy still applies. For emulator work,
use [disposable sessions](gui-emulator.md).
