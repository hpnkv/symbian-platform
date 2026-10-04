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
each collaborator can point to a local SDK. Selection order is explicit `--sdk`,
project `sdk-location.json`, then the active SDK. Changing the setting and
running `symbian app build --project PROJECT` refreshes the CMake cache as
needed. An SDK installation records its own files and digests; moving an
installed SDK directory without reinstalling does not rebase its manifest.

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
own UI. Unsupported language tags fail during packaging. The exact set of accepted tags is checked during project configuration; the
[development status](https://github.com/hpnkv/symbian-platform/blob/main/.dev/status.md)
records validation evidence.

## Installed SDK layout

| Directory | What an application uses |
| --- | --- |
| `include/platform` | Selected original Symbian headers |
| `include/c++`, `include/config` | Matching C++ runtime headers and configuration |
| `lib/armv5t`, `lib/armv6` | Guest runtime archives for each target architecture |
| `proxies` | Import libraries for selected OS services |
| `cmake` | Toolchain and native CMake targets |
| `bin`, `libexec` | Build, conversion and debugging tools |
| `sdk.json`, `digests.json` | Installed entry points and file verification |

The [SDK export reference](sdk.md) gives the fuller installed surface. The
[capability guide](../capabilities/index.md) records which native facilities
have been exercised. The current development export still depends on declared
host tools and separately supplied private firmware.
