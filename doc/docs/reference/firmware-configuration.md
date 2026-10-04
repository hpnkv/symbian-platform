# Firmware selection and configuration

This reference explains how the firmware resolver combines settings. For the first import and launch, use the [firmware guide](../guides/firmware.md).

## Hierarchy and path rules

Settings merge **per key**, in this order:

| Layer | File | Intended use |
| --- | --- | --- |
| Global | `$XDG_CONFIG_HOME/symbian/emulator.json` | User's default device and host paths |
| SDK | `SDK/emulator.json` | Optional defaults for a particular SDK profile |
| Project | `PROJECT/emulator.json` | Optional portable application selection |
| Command | `--firmware`, `--store`, `--emulator`, `--importer`, `--backend`, `--language`, `--profile` | Temporary overrides |

XDG defaults on both hosts are `~/.config`, `~/.local/share`, and `~/.cache`.
An explicitly set XDG variable must be absolute. Firmware defaults to
`$XDG_DATA_HOME/symbian/firmware`, because imported private data must survive
cache eviction. Import staging/logs use `$XDG_CACHE_HOME/symbian/firmware-imports`.
No default device is guessed from directories or installed device order.

SDK selection follows explicit `--sdk`, the project's ignored `sdk-location.json`,
then the active SDK. SDK manifests supply host-tool defaults, below preference
files. Workspace tool defaults remain available for the research build.

In JSON, relative paths resolve **against the defining file's directory**.
Command paths resolve against the current directory. `emu configure` rebases
relative command paths when saving them, preserving their meaning. Its JSON
result shows exactly the written file and settings. An omitted key inherits;
`--unset KEY` removes a key to resume inheritance. JSON `null` clears a key;
`--clear-firmware` deliberately disables an inherited device selection.
Unknown keys, invalid values and malformed higher-level files are errors;
they never trigger a silent fallback.

`symbian emu resolve --project PROJECT` reports all consulted files, their raw
settings, each effective value's origin, tool availability, the exact firmware
identity, ROM/Z/C mappings, native device metadata, integrity and selected SVC
profile. An unavailable selection appears as a canonical `selection_status`;
the inspection command itself still succeeds. Run requires a usable selection.

Prefer the default shared store and a project content ID. Configure machine
paths globally. Project/SDK configuration commands freeze aliases to exact
IDs; global defaults may intentionally use a local alias. Hand-written JSON
also supports aliases, but another machine must define the same alias.
SDK `emulator.json` is mutable configuration, excluded from SDK payload digests.
