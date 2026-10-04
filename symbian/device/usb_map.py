"""Unified native libusb inspection of a serial-matched USB device."""

from symbian.device.connection import ConnectedDevice
from symbian.device.usb_models import UsbProbe
from symbian.native import require_native


def inspect(
    device: ConnectedDevice, operation: str = "map", limit: int = 0
) -> UsbProbe:
    """Use statically linked libusb with a serial-derived identity anchor."""
    if device.identity_basis != "usb-serial":
        return UsbProbe(state="serial-identity-required")
    native = require_native()
    return UsbProbe.model_validate(
        native.inspect_usb_native(
            device.vendor_id,
            device.product_id,
            device.identity_anchor,
            operation,
            limit,
        )
    )
