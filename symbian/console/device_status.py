"""Device presence and operation state for desktop status displays."""

from typing import Literal

from pydantic import Field

from symbian.console.models import ConsoleModel
from symbian.device.connection import ConnectedDevice


class DeviceStatusView(ConsoleModel):
    """One concise, visually classified status bar message."""

    tone: Literal["connected", "pending"] = Field(
        description="Green connected or amber pending indication"
    )
    text: str = Field(description="Short device name and current state")


class DeviceOperation(ConsoleModel):
    """A request that remains pending until its result arrives."""

    token: int = Field(description="Unique local request identifier")
    label: str = Field(description="User-facing operation name")
    path: tuple[str, ...] = Field(description="Public command or probe path")
    device: ConnectedDevice | None = Field(
        default=None,
        description="Phone selected when the request began",
        exclude_if=lambda value: value is None,
    )


class DeviceStatusTracker:
    """Keep pending work visible through temporary USB disappearance."""

    def __init__(self) -> None:
        self._devices: tuple[ConnectedDevice, ...] = ()
        self._selected: ConnectedDevice | None = None
        self._active: list[DeviceOperation] = []
        self._pending_mode: ConnectedDevice | None = None
        self._uncertain: ConnectedDevice | None = None
        self._next_token = 0

    def observe(
        self, devices: tuple[ConnectedDevice, ...], selector: str | None
    ) -> None:
        """Apply one authoritative inventory and its reconciled selection."""
        self._devices = devices
        self._selected = next(
            (device for device in devices if device.selector == selector),
            None,
        )
        self._uncertain = None

    def lose_observation(self) -> None:
        """Avoid a green indicator when discovery itself failed."""
        self._devices = ()
        self._selected = None

    def begin(self, label: str, path: tuple[str, ...]) -> int:
        """Record a phone operation before it enters the worker queue."""
        self._next_token += 1
        self._active.append(
            DeviceOperation(
                token=self._next_token,
                label=label,
                path=path,
                device=self._selected,
            )
        )
        return self._next_token

    def finish(self, token: int, success: bool) -> None:
        """Resolve work and retain a successful mode-switch baseline."""
        operation = next(
            (item for item in self._active if item.token == token), None
        )
        if operation is None:
            return
        self._active.remove(operation)
        if success and operation.path == ("device", "mode", "begin"):
            self._pending_mode = operation.device
        elif success and operation.path == ("device", "mode", "verify"):
            self._pending_mode = None
        elif not success:
            self._uncertain = operation.device

    def clear_pending(self) -> None:
        """Dismiss the local hint without changing a saved mode ticket."""
        self._pending_mode = None

    @property
    def has_pending_mode(self) -> bool:
        """Whether a mode baseline awaits verification or dismissal."""
        return self._pending_mode is not None

    def view(self) -> DeviceStatusView | None:
        """Prefer active work, then a pending mode switch, then presence."""
        if self._active:
            operation = self._active[0]
            name = operation.device.product if operation.device else "phone"
            if operation.device and not self._present(operation.device):
                state = "disconnected; awaiting result"
            else:
                state = "working"
            return DeviceStatusView(
                tone="pending",
                text=f"{name} · {operation.label} ({state})",
            )
        if self._pending_mode is not None:
            return DeviceStatusView(
                tone="pending",
                text=(
                    f"{self._pending_mode.product} · "
                    "USB mode verification pending"
                ),
            )
        if self._uncertain is not None:
            return DeviceStatusView(
                tone="pending",
                text=f"{self._uncertain.product} · connection unverified",
            )
        if self._selected is None:
            return None
        device = self._selected
        return DeviceStatusView(
            tone="connected",
            text=(
                f"{device.product} · {device.vendor_id:04x}:"
                f"{device.product_id:04x} · {device.interface_profile}"
            ),
        )

    def _present(self, device: ConnectedDevice) -> bool:
        for current in self._devices:
            if current.selector == device.selector:
                return True
            if (
                device.identity_basis == "usb-serial"
                and current.identity_basis == "usb-serial"
                and current.identity_anchor == device.identity_anchor
                and current.vendor_id == device.vendor_id
            ):
                return True
        return False
