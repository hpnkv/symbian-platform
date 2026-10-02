"""Physical-device discovery and storage transfer controls."""

import hashlib
from pathlib import Path
from types import SimpleNamespace

import pytest

from symbian.cli.__main__ import _parser
from symbian.device import connection, installation
from symbian.device.linux import discover_linux
from symbian.device.policy import policy
from symbian.status import Code, StatusError


def _usb(name: str, vendor: int, product: int, children=()) -> dict:
    return {
        "IOObjectClass": "IOUSBHostDevice",
        "USB Product Name": name,
        "USB Vendor Name": "Nokia" if vendor == 0x0421 else "Other",
        "USB Serial Number": "private-phone-serial",
        "idVendor": vendor,
        "idProduct": product,
        "locationID": 12345,
        "IORegistryEntryChildren": list(children),
    }


def _tree(disk: str = "disk7") -> dict:
    return {
        "IORegistryEntryChildren": [
            _usb(
                "hub",
                0x1234,
                0x1111,
                [
                    _usb(
                        "808 PureView",
                        0x0421,
                        0x05D0,
                        [{"IORegistryEntryChildren": [{"BSD Name": disk}]}],
                    ),
                    _usb("unrelated drive", 0x2222, 0x3333),
                ],
            )
        ]
    }


def test_usb_discovery_associates_volume_with_actual_parent(tmp_path):
    installs = tmp_path / "Installs"
    installs.mkdir()
    volumes = []

    def info(disk):
        volumes.append(disk)
        return {
            "MountPoint": str(tmp_path),
            "MediaName": "S60",
            "WritableVolume": True,
        }

    devices = connection.discover_from_registry(_tree(), info)
    assert volumes == ["disk7"]
    assert len(devices) == 1
    phone = devices[0]
    assert phone.product == "808 PureView"
    assert phone.volumes[0].disk == "disk7"
    assert phone.capabilities == ("inspect-usb", "stage-sis")
    assert "private-phone-serial" not in str(phone.model_dump())
    assert phone.os_version is None and phone.rm_code is None


def test_unmounted_and_wrong_parent_do_not_gain_stage_capability(tmp_path):
    parent = _tree()
    parent["IORegistryEntryChildren"][0]["IORegistryEntryChildren"].append(
        _usb("other", 0x1111, 0x2222, [{"BSD Name": "disk8"}])
    )

    def info(disk):
        return {
            "MountPoint": str(tmp_path) if disk == "disk7" else None,
            "MediaName": "S60",
            "WritableVolume": True,
        }

    devices = connection.discover_from_registry(parent, info)
    assert len(devices) == 1
    assert devices[0].capabilities == ("inspect-usb",)


def test_selection_fails_closed_for_missing_and_multiple(monkeypatch, tmp_path):
    monkeypatch.setattr(connection, "discover", lambda: ())
    with pytest.raises(StatusError) as error:
        connection.select()
    assert error.value.code == Code.NOT_FOUND
    phone = connection.discover_from_registry(
        _tree(),
        lambda _: {
            "MountPoint": str(tmp_path),
            "MediaName": "S60",
            "WritableVolume": True,
        },
    )[0]
    monkeypatch.setattr(connection, "discover", lambda: (phone, phone))
    with pytest.raises(StatusError) as error:
        connection.select()
    assert error.value.code == Code.FAILED_PRECONDITION
    with pytest.raises(StatusError) as error:
        connection.select("usb:wrong")
    assert error.value.code == Code.NOT_FOUND


def test_staging_is_content_checked_idempotent_and_never_claims_install(
    monkeypatch, tmp_path
):
    mount = tmp_path / "mounted"
    (mount / "Installs").mkdir(parents=True)
    phone = connection.discover_from_registry(
        _tree(),
        lambda _: {
            "MountPoint": str(mount),
            "MediaName": "S60",
            "WritableVolume": True,
        },
    )[0]
    monkeypatch.setattr(installation, "select", lambda _: phone)
    monkeypatch.setattr(
        installation,
        "inspect_package",
        lambda _: {"executable_name": "sample.exe", "uid": 0xE0000001},
    )
    source = tmp_path / "sample.sis"
    source.write_bytes(b"checked-package")
    first = installation.stage_package(source, phone.selector, "disk7")
    assert first["state"] == "awaiting-on-device-install"
    assert first["transport"] == "usb-mass-storage"
    assert first["on_device_verified"] is False
    assert first["copied"] is True
    assert Path(first["staged_path"]).read_bytes() == source.read_bytes()
    assert first["sha256"] == hashlib.sha256(source.read_bytes()).hexdigest()
    second = installation.stage_package(source, phone.selector, "disk7")
    assert second["copied"] is False
    assert list((mount / "Installs").glob("*.part")) == []


def test_staging_rejects_missing_volume(monkeypatch, tmp_path):
    mount = tmp_path / "mounted"
    mount.mkdir()
    phone = connection.discover_from_registry(
        _tree(),
        lambda _: {
            "MountPoint": str(mount),
            "MediaName": "S60",
            "WritableVolume": True,
        },
    )[0]
    monkeypatch.setattr(installation, "select", lambda _: phone)
    monkeypatch.setattr(
        installation,
        "inspect_package",
        lambda _: {"executable_name": "sample.exe"},
    )
    source = tmp_path / "sample.sis"
    source.write_bytes(b"checked-package")
    with pytest.raises(StatusError) as error:
        installation.stage_package(source)
    assert error.value.code == Code.FAILED_PRECONDITION


def test_staging_refuses_low_space_before_writing(monkeypatch, tmp_path):
    mount = tmp_path / "mounted"
    (mount / "Installs").mkdir(parents=True)
    phone = connection.discover_from_registry(
        _tree(),
        lambda _: {
            "MountPoint": str(mount),
            "MediaName": "S60",
            "WritableVolume": True,
        },
    )[0]
    monkeypatch.setattr(installation, "select", lambda _: phone)
    monkeypatch.setattr(
        installation,
        "inspect_package",
        lambda _: {"executable_name": "sample.exe"},
    )
    monkeypatch.setattr(
        installation.os,
        "statvfs",
        lambda _: SimpleNamespace(f_bavail=1, f_frsize=4096),
    )
    source = tmp_path / "sample.sis"
    source.write_bytes(b"checked-package")
    with pytest.raises(StatusError) as error:
        installation.stage_package(source)
    assert error.value.code == Code.RESOURCE_EXHAUSTED
    assert list((mount / "Installs").iterdir()) == []


def test_policy_exposes_staging_but_no_direct_install_or_recovery():
    assert policy("info")["executor_available"]
    assert policy("stage-application")["executor_available"]
    assert not policy("install-application")["executor_available"]
    assert not policy("flash")["executor_available"]
    assert policy("flash")["tier"] == "outside-agent-authority"
    with pytest.raises(SystemExit):
        _parser().parse_args(["device", "flash"])


def test_linux_sysfs_mount_adapter_uses_usb_ancestor(tmp_path):
    sysfs = tmp_path / "sys"
    usb = sysfs / "bus/usb/devices/1-2"
    usb.mkdir(parents=True)
    for name, value in {
        "idVendor": "0421",
        "idProduct": "05d0",
        "manufacturer": "Nokia",
        "product": "808 PureView",
        "serial": "private-phone-serial",
    }.items():
        (usb / name).write_text(value)
    block = usb / "1-2:1.0/host0/target0/block/sdb/sdb1"
    block.mkdir(parents=True)
    (block / "ro").write_text("0")
    class_block = sysfs / "class/block"
    class_block.mkdir(parents=True)
    (class_block / "sdb1").symlink_to(block, target_is_directory=True)
    mount = tmp_path / "Phone Storage"
    (mount / "Installs").mkdir(parents=True)
    mountinfo = tmp_path / "mountinfo"
    escaped = str(mount).replace(" ", r"\040")
    mountinfo.write_text(f"42 31 8:17 / {escaped} rw - vfat /dev/sdb1 rw\n")
    devices = discover_linux(sysfs, mountinfo)
    assert len(devices) == 1
    assert devices[0].volumes[0].disk == "sdb1"
    assert devices[0].volumes[0].mount == mount
    assert devices[0].capabilities == ("inspect-usb", "stage-sis")
    assert "private-phone-serial" not in str(devices[0].model_dump())
