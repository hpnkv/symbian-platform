"""Host-observed, human-initiated USB mode transition checks."""

import os
from pathlib import Path

from pydantic import BaseModel, ConfigDict, Field, ValidationError

from symbian.device.connection import (
    ConnectedDevice,
    UsbInterface,
    discover,
    select,
)
from symbian.status import Code, StatusError


class ModeTicket(BaseModel):
    """A redacted baseline for comparing a later USB enumeration."""

    model_config = ConfigDict(extra="forbid", frozen=True)

    schema_version: int = Field(default=1, description="Ticket schema version")
    target: str = Field(description="User-selected USB mode target")
    identity_anchor: str = Field(description="Hashed device serial anchor")
    identity_basis: str = Field(description="Anchor derivation basis")
    vendor_id: int = Field(description="USB vendor identifier")
    before_product_id: int = Field(
        description="Product identifier before switch"
    )
    before_interface_profile: str = Field(
        description="Descriptor profile before switch"
    )
    before_interfaces: tuple[UsbInterface, ...] = Field(
        description="Interfaces before switch"
    )


def _signature(device: ConnectedDevice) -> tuple:
    return (
        device.product_id,
        tuple(_descriptor(item) for item in device.interfaces),
    )


def _descriptor(item: UsbInterface) -> tuple:
    """Compare fields preserved by tickets made before descriptive metadata."""
    return (
        item.configuration,
        item.number,
        item.class_code,
        item.subclass_code,
        item.protocol_code,
    )


def begin(ticket_path: Path, selector: str | None = None) -> dict:
    """Save a USB baseline before a person changes the handset's mode."""
    device = select(selector)
    if device.identity_basis != "usb-serial":
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "A USB serial identity is required for same-device verification",
        )
    if not device.interfaces:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "No USB interface descriptors available for a baseline",
        )
    ticket = ModeTicket(
        target="pc-suite",
        identity_anchor=device.identity_anchor,
        identity_basis=device.identity_basis,
        vendor_id=device.vendor_id,
        before_product_id=device.product_id,
        before_interface_profile=device.interface_profile,
        before_interfaces=device.interfaces,
    )
    flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL
    if hasattr(os, "O_NOFOLLOW"):
        flags |= os.O_NOFOLLOW
    try:
        descriptor = os.open(ticket_path, flags, 0o600)
    except FileExistsError as error:
        raise StatusError(
            Code.ALREADY_EXISTS, "Mode ticket already exists"
        ) from error
    try:
        with os.fdopen(descriptor, "w", encoding="utf-8") as output:
            output.write(ticket.model_dump_json(indent=2) + "\n")
            output.flush()
            os.fsync(output.fileno())
    except BaseException:
        ticket_path.unlink(missing_ok=True)
        raise
    return {
        "schema": "symbian.device-mode/v1",
        "state": "awaiting-handset-selection",
        "ticket": str(ticket_path),
        "target": ticket.target,
        "before_product_id": ticket.before_product_id,
        "before_interface_profile": ticket.before_interface_profile,
        "instruction": (
            "Select PC Suite / Nokia Suite USB mode on the handset, "
            "then run 'symbian device mode verify --ticket PATH'."
        ),
    }


def verify(ticket_path: Path) -> dict:
    """Compare a current enumeration with a saved serial-bound baseline."""
    try:
        with ticket_path.open("rb") as source:
            data = source.read(64 * 1024 + 1)
        if len(data) > 64 * 1024:
            raise ValueError("Mode ticket exceeds size limit")
        ticket = ModeTicket.model_validate_json(data)
    except (OSError, ValueError, ValidationError) as error:
        raise StatusError(Code.DATA_LOSS, "Invalid mode ticket") from error
    if (
        ticket.schema_version != 1
        or ticket.target != "pc-suite"
        or ticket.identity_basis != "usb-serial"
        or len(ticket.identity_anchor) != 24
    ):
        raise StatusError(Code.DATA_LOSS, "Unsupported mode ticket")
    matches = [
        device
        for device in discover()
        if device.identity_basis == "usb-serial"
        and device.identity_anchor == ticket.identity_anchor
        and device.vendor_id == ticket.vendor_id
    ]
    if len(matches) != 1:
        return {
            "schema": "symbian.device-mode/v1",
            "state": (
                "device-unavailable" if not matches else "ambiguous-device"
            ),
            "target": ticket.target,
            "same_device_verified": False,
            "target_mode_verified": False,
        }
    device = matches[0]
    before = (
        ticket.before_product_id,
        tuple(_descriptor(item) for item in ticket.before_interfaces),
    )
    changed = _signature(device) != before
    return {
        "schema": "symbian.device-mode/v1",
        "state": "usb-transition-observed" if changed else "unchanged",
        "target": ticket.target,
        "same_device_verified": True,
        "usb_transition_observed": changed,
        "target_mode_verified": False,
        "protocol_handshake_verified": False,
        "before_product_id": ticket.before_product_id,
        "after_product_id": device.product_id,
        "before_interface_profile": ticket.before_interface_profile,
        "after_interface_profile": device.interface_profile,
        "after_interfaces": [item.model_dump() for item in device.interfaces],
        "host_serial_ports": device.host_serial_ports,
        "device_selector": device.selector,
        "scope": (
            "USB enumeration only; PC Suite protocol and phone debug "
            "access remain unverified"
        ),
    }
