# Use local firmware with the emulator

[EKA2L1](https://github.com/EKA2L1/EKA2L1) runs selected Symbian software
inside a desktop emulator. A firmware image supplies the system files for a
particular device. Symbian calls its read-only system drive **Z:**. You supply
your own firmware locally; it is not part of this repository or SDK.

You can create and build an application before importing firmware. Import one
when you want to run the app in a disposable emulator instance.

EKA1 imports can open standalone emulator sessions. The Nokia 7610 profile also supports the opt-in [no-UI EKA1 process profile](eka1.md); generated
GUI applications continue to require EKA2.

## 1. Import an image

For a supported local archive, give the import a short name:

```sh
symbian firmware import '/path/to/device.7z' --name test-device --use global
symbian firmware list
```

The importer identifies its ROM and matching Z contents and stores a
content-addressed baseline. If the input contains several devices, inspect it
with `symbian firmware probe INPUT` and choose one explicitly. The [import
reference](../reference/firmware-import.md) covers ROM/RPKG, VPL, existing
EKA2L1 instances, integrity and offline transfer.

## 2. Check the selection

```sh
symbian emu resolve --project ~/dev/hello_time
```

The result shows the selected firmware identity, tool paths and whether it is
usable. An unavailable selection is reported rather than guessed. To give one
project a different imported image, use:

```sh
symbian emu configure --scope project --project ~/dev/hello_time \
  --firmware test-device
```

The [configuration reference](../reference/firmware-configuration.md)
explains global, SDK, project and command overrides.

## 3. Run the application

```sh
symbian app run --project ~/dev/hello_time
```

Each run copies the verified baseline into new writable emulator state. It
does not boot or edit your stored baseline. The emulator checks selected
imports and ARM attributes before the guest starts. If it reports a missing
service, use the [compatibility notes](../capabilities/firmware.md) to
check the profile's requirements and restrictions.

Test required services and installation policy on the target phone before
deployment.
