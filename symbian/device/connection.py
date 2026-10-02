"""Read-only USB discovery and bounded SIS staging for physical devices."""

import hashlib
import os
import plistlib
import re
import subprocess
from collections.abc import Callable
from pathlib import Path

from pydantic import BaseModel, ConfigDict

from symbian.status import Code, StatusError


class Volume(BaseModel):
    """A mounted storage volume proven to descend from a USB device."""

    model_config = ConfigDict(frozen=True)

    disk: str
    mount: Path
    media_name: str
    read_only: bool
    stage_sis: bool
    filesystem: str | None = None
    total_bytes: int | None = None
    free_bytes: int | None = None


class ConnectedDevice(BaseModel):
    """A live USB candidate; identifiers do not reveal the device serial."""

    model_config = ConfigDict(frozen=True)

    selector: str
    transport: str = "usb"
    manufacturer: str
    product: str
    vendor_id: int
    product_id: int
    location_id: int
    volumes: tuple[Volume, ...]
    capabilities: tuple[str, ...]
    os_version: str | None = None
    rm_code: str | None = None


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


def _walk_usb(
    node: dict, volume_info: Callable[[str], dict]
) -> list[ConnectedDevice]:
    found: list[ConnectedDevice] = []
    children = node.get("IORegistryEntryChildren", [])
    if not isinstance(children, list):
        return found
    if node.get("IOObjectClass") == "IOUSBHostDevice":
        media: list[dict] = []

        def collect(item: dict) -> None:
            if item.get("IOObjectClass") == "IOUSBHostDevice":
                return
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
    return {
        "schema": "symbian.device-list/v1",
        "devices": [item.model_dump(mode="json") for item in discover()],
    }


def inspect_device(selector: str | None = None) -> dict:
    """Returns currently observable USB and storage state only."""
    return {
        "schema": "symbian.device-info/v1",
        "device": select(selector).model_dump(mode="json"),
        "scope": (
            "host USB descriptors and mounted storage; phone OS state unknown"
        ),
    }
