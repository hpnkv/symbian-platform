# Firmware import and preservation

Use this reference for additional input formats, integrity checks and offline transfer. Start with the [firmware guide](../guides/firmware.md) for a single local archive.

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
