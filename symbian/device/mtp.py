"""Read-only MTP inspection via the native static libusb backend."""

from symbian.device.connection import ConnectedDevice, select
from symbian.device.usb_map import inspect as inspect_usb
from symbian.device.usb_models import UsbProbe


def inspect(device: ConnectedDevice, limit: int = 0) -> UsbProbe:
    """Read device and storage metadata, optionally bounded root handles."""
    return inspect_usb(device, "mtp", limit)


def inspect_selected(selector: str | None = None, limit: int = 0) -> UsbProbe:
    """Select an exact live device and run the bounded MTP probe."""
    return inspect(select(selector), limit)
