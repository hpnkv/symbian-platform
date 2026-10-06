# SDK and project configuration

Use this reference when a project needs another SDK, architecture, application
caption or icon. For the first project, follow [Create a standalone
application](../guides/projects.md).

## Selecting an installed SDK

The SDK install command publishes an active user setting. A project can select
another installation with `sdk-location.json`:

```json
{"sdk": "../symbian-sdk"}
```

The path may be absolute or relative to the project. It is ignored by Git, so
each collaborator can point to a local SDK. Generated projects also record the
configured `python` interpreter here so IDE launchers work outside an activated
shell. Update this local path if that Python environment moves. Selection order is explicit `--sdk`,
project `sdk-location.json`, then the active SDK. Changing the setting and
running `symbian app build --project PROJECT` refreshes the CMake cache as
needed. Native distribution manifests use relative paths and remain usable
when their directory moves. Update the project's SDK selection and active user
setting to point to the moved installation. Development source exports can
retain absolute external tool paths.

To install a downloaded [native release archive](../guides/native-distributions.md):

```sh
symbian sdk install ~/dev/symbian-sdk --archive symbian-sdk.tar.gz
```

To install from the prepared source checkout:

```sh
symbian sdk install ~/dev/symbian-sdk --workspace ~/dev/symbian
```

To make a second copy of the active SDK:

```sh
symbian sdk install ~/dev/another-sdk
```

For an existing generated project, `symbian app configure --project PROJECT
--sdk SDK` refreshes generated CMake, presets and IDE settings. Preserve
custom edits to generated files first; application source is kept.

## Application identity and menu resources

`symbian.toml` contains `[application]` menu captions and a project-relative
SVG icon. The SDK compiles registration resources with the installed `rcomp`
and packages them beside the executable. For another menu language, add
`[application.localizations.fr]` with `caption` and `short_caption`; the main
caption is the fallback. This translates the launcher entry, not your app's
own UI. Unsupported language tags fail during packaging. The exact set of accepted tags is checked during project configuration.

## Process network capability

An application using guest sockets can request the Symbian `NetworkServices`
process capability in its `[project]` table:

```toml
capabilities = ["NetworkServices"]
```

The E32 converter records this capability in the image security header. The
SDK currently rejects other capability names; an omitted list leaves the
header mask at zero. The SIS packager copies the executable's capability mask
into its file description, which the Symbian installer compares with the E32
header. A capability bit does not sign a SIS.

To sign an application package, use a matching PEM certificate and RSA private
key with `symbian package --signing-certificate CERT --signing-key KEY` plus
the usual `--project`, `--artifact` and `--output` arguments. The native
inspector verifies the signature and each packaged file. Keep signing keys
outside the project and source control. The desktop console creates a private
per-phone self-signed identity for the development agent. The phone's installer
still decides whether to grant installation, so test the package on the actual
device.

## Installed SDK layout

| Directory | What an application uses |
| --- | --- |
| `include/platform` | Selected original Symbian headers |
| `include/c++`, `include/config` | Matching C++ runtime headers and configuration |
| `lib/armv5t`, `lib/armv6` | Guest runtime archives for each target architecture |
| `proxies` | Import libraries for selected OS services |
| `cmake` | Toolchain and native CMake targets |
| `bin`, `libexec` | Build, conversion and debugging tools |
| `sdk.json` | Installed entry points; relative paths in native distributions |

The [SDK export reference](sdk.md) gives the fuller installed surface. The
[capability guide](../capabilities/index.md) describes available native facilities
and restrictions. The source export depends on declared
host tools and separately supplied private firmware.
