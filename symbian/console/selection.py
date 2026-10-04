"""Reconcile a live phone selection across USB inventory changes."""

from symbian.device.connection import ConnectedDevice


class DeviceSelection:
    """Retain a selection only when its observed identity supports it."""

    def __init__(self) -> None:
        self.current: ConnectedDevice | None = None
        self._remembered: ConnectedDevice | None = None
        self._has_selected = False

    def select(
        self, selector: str, devices: tuple[ConnectedDevice, ...]
    ) -> None:
        """Select an observed phone, ignoring stale UI events."""
        match = next(
            (item for item in devices if item.selector == selector), None
        )
        if match is None:
            return
        self.current = match
        self._remembered = match
        self._has_selected = True

    def clear(self) -> None:
        """Clear an explicit choice without auto-selecting the sole phone."""
        self.current = None
        self._remembered = None
        self._has_selected = True

    def refresh(self, devices: tuple[ConnectedDevice, ...]) -> str | None:
        """Return a live selector, or clear it after a genuine unplug.

        Serial-backed anchors survive a USB mode or address change. A port
        location is only enough to retain an uninterrupted observation.
        """
        if self.current is not None:
            exact = next(
                (
                    item
                    for item in devices
                    if item.selector == self.current.selector
                ),
                None,
            )
            if exact is not None:
                self.select(exact.selector, devices)
                return exact.selector
            self.current = None
        remembered = self._remembered
        if remembered is not None and remembered.identity_basis == "usb-serial":
            matches = [
                item
                for item in devices
                if item.identity_basis == "usb-serial"
                and item.identity_anchor == remembered.identity_anchor
                and item.vendor_id == remembered.vendor_id
            ]
            if len(matches) == 1:
                self.select(matches[0].selector, devices)
                return matches[0].selector
        if not self._has_selected and len(devices) == 1:
            self.select(devices[0].selector, devices)
            return devices[0].selector
        return None


def device_labels(devices: tuple[ConnectedDevice, ...]) -> dict[str, str]:
    """Give each phone a short, distinct label without revealing its serial."""
    labels: dict[str, str] = {}
    for device in devices:
        place = (
            f"port {device.location_id:x}"
            if device.location_id
            else f"identity {device.identity_anchor[:8]}"
        )
        base = (
            f"{device.product} · {device.vendor_id:04x}:"
            f"{device.product_id:04x} · {place}"
        )
        label = base
        if label in labels:
            label = f"{base} · {device.selector[-8:]}"
        ordinal = 2
        while label in labels:
            label = f"{base} ({ordinal})"
            ordinal += 1
        labels[label] = device.selector
    return labels
