# User asset storage

The SDK keeps machine-local user assets under `~/.symbian` on macOS and Linux.
Application sources and build outputs remain in their project directories;
firmware and reusable host assets belong to the user store.

| Location | Contents |
| --- | --- |
| `~/.symbian/config/` | Active SDK and emulator selections, global emulator preferences |
| `~/.symbian/emulators/` | Installed native EKA2L1 bundles |
| `~/.symbian/firmware/` | Imported ROM/drive objects and firmware aliases |
| `~/.symbian/signing/` | Named signing certificates and private keys |
| `~/.symbian/agent-identities/` | Private per-device pairing keys and agent packages |
| `~/.symbian/agent-observations.json` | User-reported agent state |
| `~/.symbian/cache/` | Disposable download/import staging and logs |

Choose another root before installing or configuring the SDK:

```sh
export SYMBIAN_HOME="$HOME/dev/symbian-user"
symbian emulator install
symbian emulator doctor
```

`SYMBIAN_HOME` controls all three asset categories and takes precedence over
XDG variables. Without it, an explicitly set `XDG_CONFIG_HOME`, `XDG_DATA_HOME`
or `XDG_CACHE_HOME` puts the corresponding category in that directory's
`symbian/` subdirectory. Unspecified categories retain their `~/.symbian`
defaults. Environment roots must be absolute; CMake and Python use the same
active-SDK selection policy.

Install native SDK trees wherever you choose with `symbian sdk install`.
Their active selection is saved in the configuration directory above.
No old SDK storage layout is searched or migrated automatically. Import
firmware and signing identities explicitly if you need them in a new store.
