# ROM / drive Z and device selection

Firmware is separately supplied private data. A visible SDK and a generated
application can be installed and built without it. Neither declares a Nokia
808 ROM location. Run and Debug use the same resolver and a fresh copy of the
selected baseline; they never boot the stored baseline or the original dump.
The firmware selection and code target are separate settings: a new project
defaults to ARMv6, and `symbian init --architecture armv5t` selects ARMv5T.
The imported device name is not used to guess its CPU. Build and launch reject
unsupported ELF attributes with a rebuild instruction before creating a guest
session; real hardware compatibility still requires device-specific evidence.

```sh
symbian firmware import '/path/to/C7-00.7z' --name c7 --use global
symbian firmware list
symbian init my-app --name hello --non-interactive
symbian emu resolve --project my-app
symbian app run --project my-app
```

These commands display readable summaries by default. Scripts can request the
unchanged structured result with `--output-format=json`, for example
`symbian firmware list --output-format=json`.

To create an application with a different device selection without changing
the user's default:

```sh
symbian init another-app --name another --non-interactive --firmware e71
symbian emu configure --scope project --project my-app --firmware c7
symbian app run --project my-app --firmware e6 --backend dyncom
```

`init` builds by default. Its firmware option records the exact content ID in
the new project's `emulator.json` and selects the portable runtime profile
when drive Z lacks `libpthread.dll`. Application sources and UIDs are the same
in both profiles. With no firmware option, the project inherits the default
dynamically and enables timer Tasks; `--portable-runtime` selects the smaller
profile explicitly. Run checks the actual executable's `libpthread.dll` import
against the resolved drive Z before starting the emulator.
Existing projects need no regeneration. Their `sdk-run` and `sdk-debug` wrappers
forward overrides, including IDE Program Arguments. Tools dispatching into a
project-selected SDK preserve those arguments.

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

## Import forms

The native GPL adapter links the pinned original EKA2L1 installers and readers;
Python contains configuration, storage and process policy, not firmware parsing.
The separate executable `symbian_firmware_tool` is an explicit external tool
in development exports, alongside EKA2L1. Build it through the research CMake
hook or select it with `--importer`. It is not linked into the Python wheel.

```sh
symbian firmware import DUMP.7z --name device
symbian firmware probe DUMP.7z
symbian firmware import --rom SYM.ROM --rpkg matching.rpkg --name device
symbian firmware import --rom SYM.ROM --z-drive /path/to/z/root --name device
symbian firmware import --rom self-contained.rom --name device
symbian firmware import --vpl firmware.vpl --variant 0 --name device
symbian firmware import --instance /path/to/eka2l1-instance --name device
```

Archives use libarchive content detection, including `.7z` and `.zip`, and
EKA2L1's ROM/RPKG or pre-extracted `data/roms/CODE` / `data/drives/z/CODE`
layouts. The native preflight rejects traversal, host paths, case collisions,
multiple candidate devices and expansion over 16 GiB / 200,000 entries.
ROM-named files within drive Z are data, not firmware candidates. `probe`
lists candidate mappings without extraction; it also reports preflight errors.
Ambiguous archives require explicit selection after extraction. VPL variants
are never selected silently when more than one is offered.

A ROM needing external Z fails with `FAILED_PRECONDITION` when its RPKG/Z is
missing. The upstream RPKG reader now rejects unsafe paths, huge path lengths
and truncated entries before publication. Product codes and ROM dump names
are validated before becoming host paths. These bounds are not an attestation
that every upstream parser is safe against hostile binary inputs.

Instance adoption imports one ROM and its matching extracted Z using the native
reader. It ignores the original device database, mutable C/D/E and frontend
configuration. Loading an EKA2L1 database can delete devices marked for deletion;
this workflow never loads the original database. An instance with multiple
devices requires explicit ROM/Z selection. VPL import retains its supplied
initial drive C content. Every application launch gets new writable state.

Inputs are hashed before and after import. Failed and timed-out native imports
retain request, response, logs and partial staging, and publish no object or
alias. The default timeout is 300 seconds; `--timeout` changes that bound.
Successful staging retains evidence but removes its duplicate instance.

## Identity, verification and offline transfer

Objects live at `STORE/objects/SHA256`, each with `firmware.json` and `instance/`.
Identity covers normalized native metadata and every baseline file's relative
path and SHA256. Aliases, original source paths and importer paths do not enter
the identity. `aliases.json` belongs to the store. Reusing an alias for different
bytes requires `--replace-alias`; identical imports deduplicate.

```sh
symbian firmware inspect c7
symbian firmware export c7 /transfer/c7-bundle
# On another machine; the exact project ID remains valid:
symbian firmware import --bundle /transfer/c7-bundle --name c7
```

`list` reads metadata and explicitly reports that full integrity was not checked.
`inspect`, export, bundle import and launch verify every baseline byte. Additional
or missing files, mismatched identities and links fail verification. Source
paths in provenance are historical evidence, never required for resolution.
Bundles contain private firmware and provenance, so transfer them privately.
Moving a store needs only a global `--store` configuration change; project IDs
and SDK files stay valid. There is no network download or firmware redistributor.
An independently held offline preservation copy is still required.

## Actual contracts and remaining gaps

The C++ runtime supplied by this SDK remains enabled regardless of historical
Symbian feature lists. Requirements are actual ARM EABI/EKA2 E32-V startup,
import ordinals and the services an application uses. EKA1 dumps import, but
the current starter fails before launch with a specific missing EKA2 ABI error;
an EKA1 startup/import adapter has not been implemented. Building an application
without firmware remains possible.

The GUI requires EUSER, WS32 and GDI. Missing DLLs fail before process creation.
The starter uses the default font, adapts down to 176x208 logical pixels and
supports touch, Enter/centre, Backspace and Clear/Exit softkeys. Its model keeps
twelve rows; the smallest displays show only the newest rows that fit.
Full orientation handling and all vendor/server ABIs remain unfinished.

The RM-807 executive workaround is auto-selected only for its exact verified
ROM/EUSER pair. Other dumps use the default executive map. An explicit mismatched
profile is rejected; ambient `EKA2L1_EXPERIMENTAL_SVC_PROFILE` cannot override
the resolver. Each launch report records the effective profile and mappings.

The optional `Symbian::NativeAtomics64` runtime has been executed with the
imported RM-807, RM-675 (C7) and RM-609 (E6) ROMs on both emulator CPU
backends. Their EUSER 64-bit entry points match and use LDREXD/STREXD. The
imported RM-243 (6120c) and RM-346 (E71) EUSERs lack those EABI ordinals;
selecting this profile for them is unsupported. This is firmware capability
evidence, not a claim about every device of the same model or physical phone.

Unknown font-server requests return `KErrNotSupported` and remain logged;
they no longer leave a synchronous client waiting forever. This supplies an
error contract, not an implementation of the missing font operation. Native
guest exits are checked even when the frontend exits zero. A guest reason -4
is `RESOURCE_EXHAUSTED`; -5 is `UNIMPLEMENTED`; panics and other failures retain
their actual reason and evidence path.

See the newest `STATUS.md` and `.dev/research-log.md` entries for tested devices,
screenshots, CPU backend coverage and retained failure controls. Import success
does not establish application execution, full OS boot or physical compatibility.
