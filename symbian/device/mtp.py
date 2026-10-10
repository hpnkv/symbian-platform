"""MTP inspection and checked SIS staging via the native libusb backend."""

from pathlib import Path

from symbian.device.connection import ConnectedDevice, select
from symbian.device.usb_map import inspect as inspect_usb
from symbian.device.usb_models import UsbProbe
from symbian.native import require_native
from symbian.status import Code, StatusError


def inspect(device: ConnectedDevice, limit: int = 0) -> UsbProbe:
    """Read device and storage metadata, optionally bounded root handles."""
    return inspect_usb(device, "mtp", limit)


def inspect_selected(selector: str | None = None, limit: int = 0) -> UsbProbe:
    """Select an exact live device and run the bounded MTP probe."""
    return inspect(select(selector), limit)


def _has_mtp_interface(device: ConnectedDevice) -> bool:
    """Whether this serial-matched device exposes a candidate MTP interface."""
    return device.identity_basis == "usb-serial" and any(
        (item.class_code, item.subclass_code, item.protocol_code) == (6, 1, 1)
        for item in device.interfaces
    )


def can_stage_sis(device: ConnectedDevice) -> bool:
    """Whether a candidate MTP interface could stage a SIS.

    The native transfer checks writable storage and the Installs folder again
    immediately before writing. Descriptors alone do not prove writability.
    """
    return _has_mtp_interface(device)


def read_file(
    device: ConnectedDevice,
    storage_id: int,
    relative_path: str,
    *,
    max_bytes: int = 1024 * 1024,
) -> bytes:
    """Read one bounded file by exact path from a selected MTP storage.

    The storage ID comes from ``inspect(device).storage``. The native reader
    resolves each path component, rejects ambiguous names, and checks file
    metadata against the returned byte count. It never scans outside the
    selected storage or writes to the device.
    """
    if not _has_mtp_interface(device):
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Selected device has no serial-matched MTP interface",
        )
    return require_native().read_mtp_file_native(
        device.vendor_id,
        device.product_id,
        device.identity_anchor,
        storage_id,
        relative_path,
        max_bytes,
    )


def stage_sis(
    device: ConnectedDevice, package_path: Path, filename: str, sha256: str
) -> dict:
    """Stage one checked SIS via MTP without invoking the phone installer.

    Args:
        device: Serial-matched USB device selected from live discovery.
        package_path: Host package already inspected by the SDK.
        filename: Content-addressed filename for the Installs folder.
        sha256: Expected lowercase SHA-256 digest of the SIS bytes.

    Returns:
        Storage ID, object handle, filename, upload state and number of
        pre-existing handles whose metadata the phone refused to return.
    """
    if not can_stage_sis(device):
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Selected device has no serial-matched MTP interface",
        )
    result = require_native().stage_mtp_sis_native(
        device.vendor_id,
        device.product_id,
        device.identity_anchor,
        str(package_path),
        filename,
        sha256,
    )
    return {
        "storage_id": result.storage_id,
        "object_handle": result.object_handle,
        "name": result.name,
        "copied": result.copied,
        "unreadable_children": result.unreadable_children,
    }
