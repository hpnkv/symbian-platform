"""Linux sysfs and mountinfo adapter for the shared physical-device model."""

import hashlib
import os
import re
from pathlib import Path

from symbian.device.connection import (
    ConnectedDevice,
    UsbInterface,
    Volume,
    _safe_interface_name,
    interface_profile,
)
from symbian.status import Code, StatusError


def _read(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError):
        return None


def _hex_byte(path: Path) -> int | None:
    value = _read(path)
    try:
        number = int(value, 16) if value is not None else None
    except ValueError:
        return None
    return number if number is not None and 0 <= number <= 255 else None


def _decimal_byte(path: Path) -> int | None:
    value = _read(path)
    try:
        number = int(value, 10) if value is not None else None
    except ValueError:
        return None
    return number if number is not None and 0 <= number <= 255 else None


def _interfaces(entry: Path) -> tuple[UsbInterface, ...]:
    found = []
    for path in entry.parent.glob(f"{entry.name}:*"):
        values = tuple(
            _hex_byte(path / name)
            for name in (
                "bInterfaceNumber",
                "bInterfaceClass",
                "bInterfaceSubClass",
                "bInterfaceProtocol",
            )
        )
        if any(value is None for value in values):
            continue
        found.append(
            UsbInterface(
                configuration=_hex_byte(path / "bConfigurationValue"),
                number=values[0],
                class_code=values[1],
                subclass_code=values[2],
                protocol_code=values[3],
                alternate_setting=_decimal_byte(path / "bAlternateSetting"),
                endpoint_count=_hex_byte(path / "bNumEndpoints"),
                declared_name=_safe_interface_name(_read(path / "interface")),
            )
        )
    return tuple(
        sorted(
            set(found),
            key=lambda item: (
                item.configuration if item.configuration is not None else -1,
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


def _mounts(path: Path) -> dict[str, Path]:
    """Reads kernel mountinfo, decoding its octal-escaped path components."""
    try:
        lines = path.read_text(encoding="utf-8").splitlines()
    except (OSError, UnicodeError) as error:
        raise StatusError(
            Code.UNAVAILABLE, "Cannot read Linux mountinfo"
        ) from error

    def unescape(value: str) -> str:
        return re.sub(
            r"\\([0-7]{3})",
            lambda match: chr(int(match.group(1), 8)),
            value,
        )

    mounts: dict[str, Path] = {}
    for line in lines:
        sides = line.split(" - ", 1)
        if len(sides) != 2:
            continue
        left, right = sides[0].split(), sides[1].split()
        if len(left) < 5 or len(right) < 2:
            continue
        source = unescape(right[1])
        if re.fullmatch(r"/dev/[A-Za-z0-9_]+", source):
            mounts[Path(source).name] = Path(unescape(left[4]))
    return mounts


def discover_linux(
    sysfs: Path = Path("/sys"), mountinfo: Path = Path("/proc/self/mountinfo")
) -> tuple[ConnectedDevice, ...]:
    """Joins mounted block devices to USB ancestors via real sysfs paths."""
    usb = sysfs / "bus/usb/devices"
    block = sysfs / "class/block"
    if not usb.is_dir() or not block.is_dir():
        raise StatusError(Code.UNAVAILABLE, "Linux USB sysfs is unavailable")
    mounts = _mounts(mountinfo)
    volumes: dict[Path, list[Volume]] = {}
    for disk, mount in mounts.items():
        link = block / disk
        if not link.exists() or not mount.is_dir():
            continue
        resolved = link.resolve()
        ancestor = next(
            (
                parent
                for parent in resolved.parents
                if (parent / "idVendor").is_file()
                and (parent / "idProduct").is_file()
            ),
            None,
        )
        if ancestor is None:
            continue
        # A partition's ro file reflects the block device's current state.
        read_only = _read(link / "ro") != "0"
        statistics = os.statvfs(mount)
        volumes.setdefault(ancestor, []).append(
            Volume(
                disk=disk,
                mount=mount,
                media_name=_read(link / "device/model") or "",
                read_only=read_only,
                stage_sis=(
                    not read_only
                    and (mount / "Installs").is_dir()
                    and not (mount / "Installs").is_symlink()
                ),
                total_bytes=statistics.f_blocks * statistics.f_frsize,
                free_bytes=statistics.f_bavail * statistics.f_frsize,
            )
        )
    found = []
    for entry in sorted(usb.iterdir()):
        vendor_text, product_text = _read(entry / "idVendor"), _read(
            entry / "idProduct"
        )
        if not vendor_text or not product_text:
            continue
        try:
            vendor, product = int(vendor_text, 16), int(product_text, 16)
        except ValueError:
            continue
        attached = tuple(volumes.get(entry.resolve(), []))
        if vendor != 0x0421 and not any(
            volume.media_name.casefold() == "s60" for volume in attached
        ):
            continue
        serial = _read(entry / "serial")
        identity = f"serial:{serial}" if serial else f"location:{entry.name}"
        digest = hashlib.sha256(
            f"{vendor:04x}:{product:04x}:{identity}".encode()
        ).hexdigest()[:16]
        anchor = hashlib.sha256(
            f"{vendor:04x}:{identity}".encode()
        ).hexdigest()[:24]
        interfaces = _interfaces(entry)
        found.append(
            ConnectedDevice(
                selector=f"usb:{vendor:04x}:{product:04x}:{digest}",
                manufacturer=_read(entry / "manufacturer") or "",
                product=_read(entry / "product") or "",
                vendor_id=vendor,
                product_id=product,
                location_id=0,
                volumes=attached,
                interfaces=interfaces,
                interface_profile=interface_profile(interfaces),
                identity_anchor=anchor,
                identity_basis="usb-serial" if serial else "port-location",
                capabilities=(
                    ("inspect-usb", "stage-sis")
                    if any(volume.stage_sis for volume in attached)
                    else ("inspect-usb",)
                ),
            )
        )
    return tuple(found)
