"""Linux sysfs and mountinfo adapter for the shared physical-device model."""

import hashlib
import os
import re
from pathlib import Path

from symbian.device.connection import ConnectedDevice, Volume
from symbian.status import Code, StatusError


def _read(path: Path) -> str | None:
    try:
        return path.read_text(encoding="utf-8").strip()
    except (OSError, UnicodeError):
        return None


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
        found.append(
            ConnectedDevice(
                selector=f"usb:{vendor:04x}:{product:04x}:{digest}",
                manufacturer=_read(entry / "manufacturer") or "",
                product=_read(entry / "product") or "",
                vendor_id=vendor,
                product_id=product,
                location_id=0,
                volumes=attached,
                capabilities=(
                    ("inspect-usb", "stage-sis")
                    if any(volume.stage_sis for volume in attached)
                    else ("inspect-usb",)
                ),
            )
        )
    return tuple(found)
