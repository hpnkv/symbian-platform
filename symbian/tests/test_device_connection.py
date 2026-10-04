"""Physical-device discovery and storage transfer controls."""

import hashlib
from pathlib import Path
from types import SimpleNamespace

import pytest

from symbian.cli.__main__ import _parser
from symbian.device import (
    agent_observation,
    at,
    connection,
    installation,
    mode,
    mtp,
)
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


def test_pc_suite_mtp_staging_uses_reusable_device_api(monkeypatch, tmp_path):
    phone = connection.discover_from_registry(_tree(), lambda _: {})[0]
    phone = phone.model_copy(
        update={
            "interfaces": (
                connection.UsbInterface(
                    number=0, class_code=6, subclass_code=1, protocol_code=1
                ),
            ),
        }
    )
    assert mtp.can_stage_sis(phone)
    monkeypatch.setattr(installation, "select", lambda _: phone)
    monkeypatch.setattr(
        installation,
        "inspect_package",
        lambda _: {"executable_name": "agent_service.exe", "uid": 1},
    )
    calls = []

    def stage(device, source, filename, digest):
        calls.append((device, source, filename, digest))
        return {
            "storage_id": 0x20001,
            "object_handle": 23,
            "name": filename,
            "copied": True,
            "unreadable_children": 1,
        }

    monkeypatch.setattr(mtp, "stage_sis", stage)
    source = tmp_path / "agent_service.sis"
    source.write_bytes(b"checked-sis")
    result = installation.stage_package(source, phone.selector)
    assert result["transport"] == "mtp-usb"
    assert result["volume"] == "mtp:00020001"
    assert result["object_handle"] == 23
    assert result["unreadable_children"] == 1
    assert result["on_device_verified"] is False
    assert result["staged_path"].startswith("Installs/agent_service-")
    assert calls[0][0] == phone
    assert calls[0][2].endswith(".sis")
    assert calls[0][3] == result["sha256"]


def test_agent_running_report_is_user_evidence_bound_to_serial(tmp_path):
    phone = connection.discover_from_registry(_tree(), lambda _: {})[0]
    path = tmp_path / "observations.json"
    report = agent_observation.report_running(phone, path)
    assert report.state == "running-reported"
    assert (
        agent_observation.read_for_devices((phone,), path)[phone.selector][
            "reported_at"
        ]
        == report.reported_at
    )
    switched = phone.model_copy(update={"selector": "usb:changed-mode"})
    assert "usb:changed-mode" in agent_observation.read_for_devices(
        (switched,), path
    )
    assert "private-phone-serial" not in path.read_text()
    agent_observation.clear_report(switched, path)
    assert agent_observation.read_for_devices((phone,), path) == {}


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
    usb_interface = usb.parent / "1-2:1.0"
    usb_interface.mkdir()
    for name, value in {
        "bInterfaceNumber": "00",
        "bInterfaceClass": "08",
        "bInterfaceSubClass": "06",
        "bInterfaceProtocol": "50",
        "bConfigurationValue": "01",
        "bAlternateSetting": "10",
        "bNumEndpoints": "02",
        "interface": "Nokia Storage",
    }.items():
        (usb_interface / name).write_text(value)
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
    assert devices[0].interface_profile == "mass-storage"
    assert devices[0].interfaces[0].protocol_code == 0x50
    assert devices[0].interfaces[0].alternate_setting == 10
    assert devices[0].interfaces[0].endpoint_count == 2
    assert devices[0].interfaces[0].declared_name == "Nokia Storage"
    assert devices[0].identity_basis == "usb-serial"
    assert "private-phone-serial" not in str(devices[0].model_dump())


def test_mode_ticket_requires_same_serial_and_observes_usb_transition(
    monkeypatch, tmp_path
):
    storage = _usb(
        "808 PureView",
        0x0421,
        0x05D0,
        [
            {
                "IOObjectClass": "IOUSBHostInterface",
                "bConfigurationValue": 1,
                "bInterfaceNumber": 0,
                "bInterfaceClass": 8,
                "bInterfaceSubClass": 6,
                "bInterfaceProtocol": 80,
            }
        ],
    )
    suite = _usb(
        "808 PureView",
        0x0421,
        0x05D1,
        [
            {
                "IOObjectClass": "IOUSBHostInterface",
                "bConfigurationValue": 1,
                "bInterfaceNumber": 0,
                "bInterfaceClass": 2,
                "bInterfaceSubClass": 2,
                "bInterfaceProtocol": 1,
                "IORegistryEntryChildren": [
                    {
                        "IOObjectClass": "IOSerialBSDClient",
                        "IOCalloutDevice": "/dev/cu.usbmodem141202",
                    }
                ],
            }
        ],
    )
    baseline = connection.discover_from_registry(
        {"IORegistryEntryChildren": [storage]}, lambda _: {}
    )[0]
    after = connection.discover_from_registry(
        {"IORegistryEntryChildren": [suite]}, lambda _: {}
    )[0]
    monkeypatch.setattr(mode, "select", lambda _: baseline)
    monkeypatch.setattr(mode, "discover", lambda: (baseline,))
    ticket = tmp_path / "mode.json"
    assert mode.begin(ticket)["state"] == "awaiting-handset-selection"
    assert "private-phone-serial" not in ticket.read_text()
    assert ticket.stat().st_mode & 0o777 == 0o600
    with pytest.raises(StatusError) as error:
        mode.begin(ticket)
    assert error.value.code == Code.ALREADY_EXISTS
    assert mode.verify(ticket)["state"] == "unchanged"
    monkeypatch.setattr(mode, "discover", lambda: (after,))
    result = mode.verify(ticket)
    assert result["state"] == "usb-transition-observed"
    assert result["same_device_verified"] is True
    assert result["target_mode_verified"] is False
    assert result["host_serial_ports"] == ("/dev/cu.usbmodem141202",)
    impostor = suite | {"USB Serial Number": "different-phone"}
    other = connection.discover_from_registry(
        {"IORegistryEntryChildren": [impostor]}, lambda _: {}
    )[0]
    monkeypatch.setattr(mode, "discover", lambda: (other,))
    assert mode.verify(ticket)["state"] == "device-unavailable"


def test_mode_ticket_rejects_location_only_identity(monkeypatch, tmp_path):
    device = connection.discover_from_registry(
        {"IORegistryEntryChildren": [_usb("808", 0x0421, 0x05D0)]},
        lambda _: {},
    )[0]
    monkeypatch.setattr(
        mode,
        "select",
        lambda _: device.model_copy(update={"identity_basis": "port-location"}),
    )
    with pytest.raises(StatusError) as error:
        mode.begin(tmp_path / "mode.json")
    assert error.value.code == Code.FAILED_PRECONDITION


def test_interface_names_and_host_binding_are_attached_to_exact_interface():
    serial_client = {
        "IOObjectClass": "IOSerialBSDClient",
        "IOCalloutDevice": "/dev/cu.usbmodem141202",
    }
    phone = connection.discover_from_registry(
        {
            "IORegistryEntryChildren": [
                _usb(
                    "808 PureView",
                    0x0421,
                    0x05D1,
                    [
                        {
                            "IOObjectClass": "IOUSBHostInterface",
                            "IORegistryEntryName": "MTP",
                            "bInterfaceNumber": 0,
                            "bInterfaceClass": 6,
                            "bInterfaceSubClass": 1,
                            "bInterfaceProtocol": 1,
                            "bNumEndpoints": 3,
                            "bAlternateSetting": 0,
                        },
                        {
                            "IOObjectClass": "IOUSBHostInterface",
                            "IORegistryEntryName": "CDC Data Interface",
                            "bInterfaceNumber": 2,
                            "bInterfaceClass": 10,
                            "bInterfaceSubClass": 0,
                            "bInterfaceProtocol": 0,
                            "bNumEndpoints": 2,
                            "IORegistryEntryChildren": [
                                {
                                    "IOObjectClass": "AppleUSBACMData",
                                    "IORegistryEntryChildren": [serial_client],
                                }
                            ],
                        },
                    ],
                )
            ]
        },
        lambda _: {},
    )[0]
    imaging, data = phone.interfaces
    assert imaging.declared_name == "MTP"
    assert imaging.function == "Still imaging / PTP transport"
    assert imaging.endpoint_count == 3
    assert imaging.host_serial_port is None
    assert data.declared_name is None
    assert data.host_driver == "AppleUSBACMData"
    assert data.host_serial_port == "/dev/cu.usbmodem141202"
    assert data.model_dump()["function"] == "CDC data interface"


def test_descriptive_interface_metadata_does_not_forge_mode_transition(
    monkeypatch, tmp_path
):
    interface = connection.UsbInterface(
        number=0, class_code=6, subclass_code=1, protocol_code=1
    )
    baseline = connection.ConnectedDevice(
        selector="usb:0421:05d1:test",
        manufacturer="Nokia",
        product="808 PureView",
        vendor_id=0x0421,
        product_id=0x05D1,
        location_id=1,
        volumes=(),
        interfaces=(interface,),
        identity_anchor="a" * 24,
        identity_basis="usb-serial",
        capabilities=("inspect-usb",),
    )
    enriched = baseline.model_copy(
        update={
            "interfaces": (
                interface.model_copy(
                    update={"declared_name": "MTP", "endpoint_count": 3}
                ),
            )
        }
    )
    monkeypatch.setattr(mode, "select", lambda _: baseline)
    monkeypatch.setattr(mode, "discover", lambda: (enriched,))
    ticket = tmp_path / "mode.json"
    mode.begin(ticket)
    assert mode.verify(ticket)["state"] == "unchanged"


def test_info_queries_at_only_for_observed_808_pc_suite_port(monkeypatch):
    phone = connection.discover_from_registry(
        {
            "IORegistryEntryChildren": [
                _usb(
                    "808 PureView",
                    0x0421,
                    0x05D1,
                    [
                        {
                            "IOObjectClass": "IOUSBHostInterface",
                            "bInterfaceNumber": 1,
                            "bInterfaceClass": 2,
                            "bInterfaceSubClass": 2,
                            "bInterfaceProtocol": 1,
                            "IORegistryEntryChildren": [
                                {
                                    "IOObjectClass": "IOSerialBSDClient",
                                    "IOCalloutDevice": "/dev/cu.usbmodem141202",
                                }
                            ],
                        },
                        {
                            "IOObjectClass": "IOUSBHostInterface",
                            "bInterfaceNumber": 2,
                            "bInterfaceClass": 10,
                            "bInterfaceSubClass": 0,
                            "bInterfaceProtocol": 0,
                        },
                    ],
                )
            ]
        },
        lambda _: {},
    )[0]
    monkeypatch.setattr(connection, "select", lambda _: phone)
    calls = []

    def fake_probe(port, include_status=False):
        calls.append(port)
        return {
            "state": "at-ready",
            "transport": "cdc-acm",
            "queries": {
                "manufacturer": {"state": "ok", "value": "Nokia"},
                "model": {"state": "ok", "value": "Nokia 808 PureView"},
                "revision": {
                    "state": "ok",
                    "value": "113.010.1508 2013-01-02 RM-807 (c) Nokia",
                },
            },
        }

    monkeypatch.setattr(at, "probe", fake_probe)
    from symbian.device import usb_map

    monkeypatch.setattr(
        usb_map,
        "inspect",
        lambda device, operation="map", limit=0: {"state": "observed"},
    )
    assert "protocol_probe" not in connection.inspect_device(
        probe_protocol=False
    )
    assert not calls
    result = connection.inspect_device()
    assert calls == ["/dev/cu.usbmodem141202"]
    assert result["reported_identity"]["rm_code"] == "RM-807"
    assert result["reported_identity"]["firmware_revision"] == "113.010.1508"
    assert "os_version" not in result["device"]
