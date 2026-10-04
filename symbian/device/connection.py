"""Read-only USB discovery and bounded SIS staging for physical devices."""

import hashlib
import os
import plistlib
import re
import subprocess
from collections.abc import Callable
from pathlib import Path

from pydantic import BaseModel, ConfigDict, Field, computed_field

from symbian.device.at import AtProbe
from symbian.device.usb_models import UsbProbe
from symbian.status import Code, StatusError


class Volume(BaseModel):
    """A mounted storage volume proven to descend from a USB device."""

    model_config = ConfigDict(frozen=True)

    disk: str = Field(description="Host disk identifier")
    mount: Path = Field(description="Mounted filesystem path")
    media_name: str = Field(description="USB media name")
    read_only: bool = Field(description="Host mount is read-only")
    stage_sis: bool = Field(description="Volume is eligible for SIS staging")
    filesystem: str | None = Field(
        default=None,
        description="Filesystem name",
        exclude_if=lambda value: value is None,
    )
    total_bytes: int | None = Field(
        default=None,
        description="Total filesystem capacity",
        exclude_if=lambda value: value is None,
    )
    free_bytes: int | None = Field(
        default=None,
        description="Available filesystem capacity",
        exclude_if=lambda value: value is None,
    )


class UsbInterface(BaseModel):
    """Interface descriptor values observed by the host USB stack."""

    model_config = ConfigDict(frozen=True)

    configuration: int | None = Field(
        default=None,
        description="Configuration value",
        exclude_if=lambda value: value is None,
    )
    number: int = Field(description="Interface number")
    class_code: int = Field(description="USB class code")
    subclass_code: int = Field(description="USB subclass code")
    protocol_code: int = Field(description="USB protocol code")
    alternate_setting: int | None = Field(
        default=None,
        description="Alternate setting",
        exclude_if=lambda value: value is None,
    )
    endpoint_count: int | None = Field(
        default=None,
        description="Endpoint count",
        exclude_if=lambda value: value is None,
    )
    declared_name: str | None = Field(
        default=None,
        description="Host-reported interface name",
        exclude_if=lambda value: value is None,
    )
    host_driver: str | None = Field(
        default=None,
        description="Host driver",
        exclude_if=lambda value: value is None,
    )
    host_serial_port: str | None = Field(
        default=None,
        description="Host serial port path",
        exclude_if=lambda value: value is None,
    )

    @computed_field
    @property
    def function(self) -> str:
        """Return a USB-standard role, not proof of an application protocol."""
        codes = (self.class_code, self.subclass_code, self.protocol_code)
        known = {
            (6, 1, 1): "Still imaging / PTP transport",
            (8, 6, 80): "Mass storage / SCSI over USB bulk transport",
            (2, 2, 1): "CDC ACM control / AT command protocol",
            (2, 2, 255): "CDC ACM control / vendor protocol",
            (2, 8, 0): "CDC wireless handset control",
            (2, 11, 0): "CDC OBEX control",
            (10, 0, 0): "CDC data interface",
        }
        if codes in known:
            return known[codes]
        if self.class_code == 2:
            return (
                "CDC control / unrecognized subclass "
                f"0x{self.subclass_code:02x}"
            )
        if self.class_code == 255:
            return "Vendor-specific USB interface"
        return f"USB class 0x{self.class_code:02x} (unclassified)"


def interface_profile(interfaces: tuple[UsbInterface, ...]) -> str:
    """Names only USB class evidence, not the handset's selected UI mode."""
    if not interfaces:
        return "unknown"
    profiles = set()
    for item in interfaces:
        if (item.class_code, item.subclass_code, item.protocol_code) == (
            8,
            6,
            80,
        ):
            profiles.add("mass-storage")
        elif (item.class_code, item.subclass_code) == (6, 1):
            profiles.add("still-image")
        elif item.class_code == 255:
            profiles.add("vendor-specific")
        else:
            profiles.add("other")
    return next(iter(profiles)) if len(profiles) == 1 else "composite"


class ConnectedDevice(BaseModel):
    """A live USB candidate; identifiers do not reveal the device serial."""

    model_config = ConfigDict(frozen=True)

    selector: str = Field(description="Serial-redacted selection token")
    transport: str = Field(default="usb", description="Physical transport")
    manufacturer: str = Field(description="Host-reported manufacturer")
    product: str = Field(description="Host-reported product")
    vendor_id: int = Field(description="USB vendor identifier")
    product_id: int = Field(description="USB product identifier")
    location_id: int = Field(description="Host USB location identifier")
    volumes: tuple[Volume, ...] = Field(
        default=(),
        description="Mounted volumes",
        exclude_if=lambda value: not value,
    )
    interfaces: tuple[UsbInterface, ...] = Field(
        default=(),
        description="Observed interfaces",
        exclude_if=lambda value: not value,
    )
    interface_profile: str = Field(
        default="unknown", description="Descriptor-derived profile"
    )
    host_serial_ports: tuple[str, ...] = Field(
        default=(),
        description="Host serial ports",
        exclude_if=lambda value: not value,
    )
    identity_anchor: str = Field(
        default="",
        description="Hashed serial anchor",
        exclude_if=lambda value: not value,
    )
    identity_basis: str = Field(
        default="unknown", description="Anchor derivation basis"
    )
    capabilities: tuple[str, ...] = Field(
        default=(),
        description="Available host operations",
        exclude_if=lambda value: not value,
    )
    os_version: str | None = Field(
        default=None,
        description="Observed OS version",
        exclude_if=lambda value: value is None,
    )
    rm_code: str | None = Field(
        default=None,
        description="Device model code",
        exclude_if=lambda value: value is None,
    )


class DeviceListResult(BaseModel):
    """Serial-redacted device inventory for the CLI output boundary."""

    model_config = ConfigDict(frozen=True)

    schema_name: str = Field(
        default="symbian.device-list/v1",
        description="Output schema",
        serialization_alias="schema",
    )
    devices: tuple[ConnectedDevice, ...] = Field(
        default=(), description="Current supported devices"
    )


class ReportedIdentity(BaseModel):
    """Identity safely reported by read-only AT commands."""

    model_config = ConfigDict(frozen=True)

    source: str = Field(
        default="read-only AT+CGMI/CGMM/CGMR", description="Evidence source"
    )
    manufacturer: str | None = Field(
        default=None,
        description="Reported manufacturer",
        exclude_if=lambda value: value is None,
    )
    model: str | None = Field(
        default=None,
        description="Reported model",
        exclude_if=lambda value: value is None,
    )
    firmware_revision: str | None = Field(
        default=None,
        description="Reported firmware revision",
        exclude_if=lambda value: value is None,
    )
    firmware_date: str | None = Field(
        default=None,
        description="Reported firmware date",
        exclude_if=lambda value: value is None,
    )
    rm_code: str | None = Field(
        default=None,
        description="Reported model code",
        exclude_if=lambda value: value is None,
    )


class DeviceInfoResult(BaseModel):
    """Host inspection and opt-in protocol evidence for one device."""

    model_config = ConfigDict(frozen=True)

    schema_name: str = Field(
        default="symbian.device-info/v1",
        description="Output schema",
        serialization_alias="schema",
    )
    device: ConnectedDevice = Field(description="Selected live device")
    pc_suite_usb_candidate: bool = Field(
        default=False, description="Descriptor-only PC Suite layout candidate"
    )
    scope: str = Field(
        default=(
            "host USB descriptors and mounted storage; "
            "phone OS state unknown"
        ),
        description="Evidence boundary",
    )
    usb_map: "UsbProbe" = Field(description="Native USB descriptor map")
    mtp_probe: "UsbProbe | None" = Field(
        default=None,
        description="Optional MTP probe",
        exclude_if=lambda value: value is None,
    )
    obex_probe: "UsbProbe | None" = Field(
        default=None,
        description="Optional OBEX probe",
        exclude_if=lambda value: value is None,
    )
    protocol_probe: "AtProbe | None" = Field(
        default=None,
        description="Optional AT probe",
        exclude_if=lambda value: value is None,
    )
    reported_identity: ReportedIdentity | None = Field(
        default=None,
        description="Safely parsed AT identity",
        exclude_if=lambda value: value is None,
    )


def _plist_command(argv: list[str]) -> dict:
    """Executes a read-only Apple inventory command with a fixed timeout."""
    try:
        result = subprocess.run(
            argv, capture_output=True, timeout=15, check=False
        )
    except (OSError, subprocess.TimeoutExpired) as error:
        raise StatusError(
            Code.UNAVAILABLE, f"Device inventory failed: {argv[0]}"
        ) from error
    if result.returncode or len(result.stdout) > 64 * 1024 * 1024:
        raise StatusError(
            Code.UNAVAILABLE, f"Device inventory failed: {argv[0]}"
        )
    try:
        return plistlib.loads(result.stdout)
    except (ValueError, TypeError) as error:
        raise StatusError(
            Code.DATA_LOSS, "Invalid device inventory plist"
        ) from error


def _volume_info(disk: str) -> dict:
    if not re.fullmatch(r"disk[0-9]+(?:s[0-9]+)?", disk):
        raise StatusError(Code.DATA_LOSS, "Invalid disk identifier in USB tree")
    return _plist_command(["diskutil", "info", "-plist", f"/dev/{disk}"])


def _is_stage_volume(volume: Volume) -> bool:
    installs = volume.mount / "Installs"
    return (
        not volume.read_only
        and volume.mount.is_dir()
        and installs.is_dir()
        and not installs.is_symlink()
    )


def _candidate(node: dict, media: list[dict]) -> bool:
    """Accept Nokia USB devices and other USB devices exposing S60 media."""
    return node.get("idVendor") == 0x0421 or any(
        str(item.get("media_name", "")).casefold() == "s60" for item in media
    )


def _safe_interface_name(value: object) -> str | None:
    if not isinstance(value, str) or not 1 <= len(value) <= 64:
        return None
    if value in (
        "IOUSBHostInterface",
        "CDC Comms Interface",
        "CDC Data Interface",
    ) or any(not 32 <= ord(character) <= 126 for character in value):
        return None
    if re.search(r"\b[0-9]{14,16}\b", value):
        return None
    return value


def _interface_services(item: dict) -> tuple[str | None, str | None]:
    """Find a host driver and callout port below one USB interface."""
    drivers: set[str] = set()
    ports: set[str] = set()

    def visit(node: dict) -> None:
        kind = node.get("IOObjectClass")
        if kind in ("IOUSBHostDevice", "IOUSBHostInterface"):
            return
        if kind in ("AppleUSBACMControl", "AppleUSBACMData"):
            drivers.add(kind)
        if kind == "IOSerialBSDClient":
            port = node.get("IOCalloutDevice")
            if isinstance(port, str) and re.fullmatch(
                r"/dev/cu\.[A-Za-z0-9._-]+", port
            ):
                ports.add(port)
        for child in node.get("IORegistryEntryChildren", []):
            if isinstance(child, dict):
                visit(child)

    for child in item.get("IORegistryEntryChildren", []):
        if isinstance(child, dict):
            visit(child)
    return (
        next(iter(drivers)) if len(drivers) == 1 else None,
        next(iter(ports)) if len(ports) == 1 else None,
    )


def _walk_usb(
    node: dict, volume_info: Callable[[str], dict]
) -> list[ConnectedDevice]:
    found: list[ConnectedDevice] = []
    children = node.get("IORegistryEntryChildren", [])
    if not isinstance(children, list):
        return found
    if node.get("IOObjectClass") == "IOUSBHostDevice":
        media: list[dict] = []
        interfaces: list[UsbInterface] = []
        serial_ports: list[str] = []

        def collect(item: dict) -> None:
            if item.get("IOObjectClass") == "IOUSBHostDevice":
                return
            if item.get("IOObjectClass") == "IOUSBHostInterface":
                values = (
                    item.get("bInterfaceNumber"),
                    item.get("bInterfaceClass"),
                    item.get("bInterfaceSubClass"),
                    item.get("bInterfaceProtocol"),
                )
                configuration = item.get("bConfigurationValue")
                if all(
                    type(value) is int and 0 <= value <= 255 for value in values
                ):
                    driver, port = _interface_services(item)
                    if port:
                        serial_ports.append(port)
                    alternate = item.get("bAlternateSetting")
                    endpoints = item.get("bNumEndpoints")
                    interfaces.append(
                        UsbInterface(
                            configuration=(
                                configuration
                                if type(configuration) is int
                                and 0 <= configuration <= 255
                                else None
                            ),
                            number=values[0],
                            class_code=values[1],
                            subclass_code=values[2],
                            protocol_code=values[3],
                            alternate_setting=(
                                alternate
                                if type(alternate) is int
                                and 0 <= alternate <= 255
                                else None
                            ),
                            endpoint_count=(
                                endpoints
                                if type(endpoints) is int
                                and 0 <= endpoints <= 255
                                else None
                            ),
                            declared_name=_safe_interface_name(
                                item.get("IORegistryEntryName")
                            ),
                            host_driver=driver,
                            host_serial_port=port,
                        )
                    )
            if item.get("IOObjectClass") == "IOSerialBSDClient":
                port = item.get("IOCalloutDevice")
                if isinstance(port, str) and re.fullmatch(
                    r"/dev/cu\.[A-Za-z0-9._-]+", port
                ):
                    serial_ports.append(port)
            disk = item.get("BSD Name")
            if isinstance(disk, str) and re.fullmatch(
                r"disk[0-9]+(?:s[0-9]+)?", disk
            ):
                info = volume_info(disk)
                mount = info.get("MountPoint")
                if isinstance(mount, str) and mount:
                    media.append(
                        {
                            "disk": disk,
                            "mount": mount,
                            "media_name": info.get("MediaName", ""),
                            "read_only": not bool(
                                info.get("WritableVolume", False)
                            ),
                            "filesystem": info.get("FilesystemName"),
                            "total_bytes": info.get("VolumeSize"),
                            "free_bytes": info.get("FreeSpace"),
                        }
                    )
            for child in item.get("IORegistryEntryChildren", []):
                if isinstance(child, dict):
                    collect(child)

        for child in children:
            if isinstance(child, dict):
                collect(child)
        if _candidate(node, media):
            vendor = node.get("idVendor")
            product = node.get("idProduct")
            location = node.get("locationID", 0)
            if all(type(value) is int for value in (vendor, product, location)):
                serial = node.get("USB Serial Number") or node.get(
                    "kUSBSerialNumberString"
                )
                identity = (
                    f"serial:{serial}"
                    if isinstance(serial, str) and serial
                    else f"location:{location:08x}"
                )
                digest = hashlib.sha256(
                    f"{vendor:04x}:{product:04x}:{identity}".encode()
                ).hexdigest()[:16]
                basis = (
                    "usb-serial"
                    if isinstance(serial, str) and serial
                    else "port-location"
                )
                anchor = hashlib.sha256(
                    f"{vendor:04x}:{identity}".encode()
                ).hexdigest()[:24]
                ordered_interfaces = tuple(
                    sorted(
                        set(interfaces),
                        key=lambda item: (
                            (
                                item.configuration
                                if item.configuration is not None
                                else -1
                            ),
                            item.number,
                            (
                                item.alternate_setting
                                if item.alternate_setting is not None
                                else -1
                            ),
                            item.class_code,
                            item.subclass_code,
                            item.protocol_code,
                        ),
                    )
                )
                volumes = tuple(
                    Volume(
                        disk=item["disk"],
                        mount=Path(item["mount"]),
                        media_name=str(item["media_name"]),
                        read_only=item["read_only"],
                        stage_sis=False,
                        filesystem=item["filesystem"],
                        total_bytes=item["total_bytes"],
                        free_bytes=item["free_bytes"],
                    )
                    for item in media
                )
                volumes = tuple(
                    volume.model_copy(
                        update={"stage_sis": _is_stage_volume(volume)}
                    )
                    for volume in volumes
                )
                found.append(
                    ConnectedDevice(
                        selector=f"usb:{vendor:04x}:{product:04x}:{digest}",
                        manufacturer=str(node.get("USB Vendor Name", "")),
                        product=str(node.get("USB Product Name", "")),
                        vendor_id=vendor,
                        product_id=product,
                        location_id=location,
                        volumes=volumes,
                        interfaces=ordered_interfaces,
                        interface_profile=interface_profile(ordered_interfaces),
                        host_serial_ports=tuple(sorted(set(serial_ports))),
                        identity_anchor=anchor,
                        identity_basis=basis,
                        capabilities=(
                            ("inspect-usb", "stage-sis")
                            if any(volume.stage_sis for volume in volumes)
                            else ("inspect-usb",)
                        ),
                    )
                )
    for child in children:
        if isinstance(child, dict):
            found.extend(_walk_usb(child, volume_info))
    return found


def discover_from_registry(
    registry: dict, volume_info: Callable[[str], dict]
) -> tuple[ConnectedDevice, ...]:
    """Maps USB storage to its actual parent device, not a guessed label."""
    return tuple(
        sorted(
            _walk_usb(registry, volume_info), key=lambda device: device.selector
        )
    )


def discover() -> tuple[ConnectedDevice, ...]:
    """Lists supported physical connection candidates without opening them."""
    if os.uname().sysname == "Linux":
        from symbian.device.linux import discover_linux

        return discover_linux()
    if os.uname().sysname != "Darwin":
        raise StatusError(
            Code.UNIMPLEMENTED,
            "Physical USB discovery supports macOS and Linux",
        )
    registry = _plist_command(["ioreg", "-p", "IOService", "-a", "-l", "-w0"])
    return discover_from_registry(registry, _volume_info)


def select(selector: str | None = None) -> ConnectedDevice:
    """Selects exactly one current device; ambiguous matches fail closed."""
    devices = discover()
    matches = [
        item
        for item in devices
        if selector is None or item.selector == selector
    ]
    if not matches:
        raise StatusError(
            Code.NOT_FOUND,
            "No matching USB Symbian device; run 'symbian device list'",
        )
    if len(matches) != 1:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Multiple devices connected; pass --device from device list",
        )
    return matches[0]


def list_devices() -> dict:
    """Returns a structured, serial-redacted device inventory."""
    return DeviceListResult(devices=discover()).model_dump(
        mode="json", by_alias=True
    )


def inspect_device(
    selector: str | None = None,
    probe_protocol: bool = True,
    usb_map: bool = True,
    at_status: bool = False,
    mtp: bool = False,
    mtp_list: int = 0,
    obex_connect: bool = False,
) -> dict:
    """Return host USB state and bounded AT identity when available."""
    device = select(selector)
    pc_suite_candidate = (
        device.vendor_id == 0x0421
        and device.product_id == 0x05D1
        and device.product == "808 PureView"
        and any(
            (item.class_code, item.subclass_code, item.protocol_code)
            == (2, 2, 1)
            for item in device.interfaces
        )
        and any(item.class_code == 10 for item in device.interfaces)
        and len(device.host_serial_ports) == 1
    )
    from symbian.device.usb_map import inspect as inspect_usb

    observed_map = inspect_usb(device)
    usb_probe = UsbProbe.model_validate(observed_map)
    mtp_probe = None
    if mtp or mtp_list:
        mtp_probe = UsbProbe.model_validate(
            inspect_usb(device, "mtp", mtp_list)
        )
    obex_probe = None
    if obex_connect:
        obex_probe = UsbProbe.model_validate(inspect_usb(device, "obex"))
    protocol_probe = None
    reported_identity = None
    scope = "host USB descriptors and mounted storage; phone OS state unknown"
    if pc_suite_candidate and probe_protocol:
        from symbian.device.at import probe

        protocol_probe = AtProbe.model_validate(
            probe(device.host_serial_ports[0], include_status=at_status)
        )
        if protocol_probe.state == "at-ready" and protocol_probe.queries:
            queries = protocol_probe.queries
            revision = queries.revision.value
            revision_parts = (
                re.match(
                    r"^(\d{3}\.\d{3}\.\d{4})\s+"
                    r"(\d{4}-\d{2}-\d{2})\s+(RM-\d+)\b",
                    revision,
                )
                if revision
                else None
            )
            reported_identity = ReportedIdentity(
                manufacturer=queries.manufacturer.value,
                model=queries.model.value,
                firmware_revision=(
                    revision_parts.group(1) if revision_parts else None
                ),
                firmware_date=(
                    revision_parts.group(2) if revision_parts else None
                ),
                rm_code=revision_parts.group(3) if revision_parts else None,
            )
            scope = (
                "host USB and read-only AT identity; PC Suite protocol, "
                "Symbian OS build and debugging access remain unknown"
            )
    return DeviceInfoResult(
        device=device,
        pc_suite_usb_candidate=pc_suite_candidate,
        scope=scope,
        usb_map=usb_probe,
        mtp_probe=mtp_probe,
        obex_probe=obex_probe,
        protocol_probe=protocol_probe,
        reported_identity=reported_identity,
    ).model_dump(mode="json", by_alias=True)
