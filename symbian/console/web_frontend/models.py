"""Typed messages produced by the desktop webview bridge."""

from typing import Literal

from pydantic import Field

from symbian.console.device_status import DeviceStatusView
from symbian.console.models import (
    CommandResult,
    CommandSpec,
    ConsoleContext,
    ConsoleModel,
    OutcomeSummary,
)
from symbian.console.presentation import TaskPresentation, TaskStep
from symbian.device.connection import DeviceInfoResult
from symbian.device.usb_models import UsbDeviceDescriptor


class DesktopTask(ConsoleModel):
    """One runnable action with its guided UI description."""

    command: CommandSpec = Field(description="Public CLI operation")
    presentation: TaskPresentation = Field(
        description="Purpose and action label"
    )
    steps: tuple[TaskStep, ...] = Field(description="Grouped wizard inputs")


class DesktopCatalog(ConsoleModel):
    """All public actions exposed to the desktop frontend."""

    tasks: tuple[DesktopTask, ...] = Field(description="Guided public actions")


class FirmwareDevice(ConsoleModel):
    """Device identity declared by an imported firmware manifest."""

    model: str = Field(description="Device model declared by the manifest")
    manufacturer: str = Field(description="Declared manufacturer")
    firmware_code: str = Field(description="Firmware product code")
    symbian_version: str = Field(description="Declared Symbian generation")
    kernel: str = Field(description="Declared EKA kernel generation")
    machine_uid: int = Field(description="Declared machine UID")
    rom: str = Field(description="ROM path within the imported object")
    z_drive: str = Field(description="Z drive path within the imported object")
    c_drive: str = Field(description="C drive path within the imported object")


class FirmwareRecord(ConsoleModel):
    """One imported firmware identity and its local aliases."""

    firmware: str = Field(description="Content-addressed firmware identity")
    aliases: tuple[str, ...] = Field(
        default=(), description="Human aliases for this identity"
    )
    device: FirmwareDevice = Field(description="Manifest device metadata")
    directory: str = Field(description="Object directory in the content store")


class FirmwareLibrary(ConsoleModel):
    """Read-only catalog from the currently selected content store."""

    store: str = Field(description="Effective firmware content store path")
    objects: tuple[FirmwareRecord, ...] = Field(
        default=(), description="Imported firmware records"
    )
    integrity_verified: bool = Field(
        default=False,
        description="Whether full file bytes were verified by this listing",
    )


class UsbInventoryItem(UsbDeviceDescriptor):
    """Native host descriptor with a human USB class label."""

    class_name: str = Field(description="USB class name and exact code")


class UsbTopology(ConsoleModel):
    """Cheap serial-free USB observation used for connection changes."""

    devices: tuple[UsbDeviceDescriptor, ...] = Field(
        default=(), description="Current native host USB descriptors"
    )


class HostSelectionRequest(ConsoleModel):
    """One explicit desktop context selection."""

    kind: Literal["workspace", "project", "sdk"] = Field(
        description="Host context item to select"
    )
    path: str = Field(
        default="",
        description=(
            "Directory or SDK manifest path; empty clears project or SDK"
        ),
        exclude_if=lambda value: not value,
    )


class FormDefault(ConsoleModel):
    """One actual effective value for a selected form field."""

    name: str = Field(description="Catalog argument destination")
    value: str = Field(description="Resolved current value")


class FormDefaults(ConsoleModel):
    """Context-aware defaults for one selected action."""

    path: tuple[str, ...] = Field(description="Public CLI command path")
    values: tuple[FormDefault, ...] = Field(
        default=(), description="Effective field values"
    )


class FormValues(ConsoleModel):
    """User-entered values for one guided action."""

    path: tuple[str, ...] = Field(description="Public CLI command path")
    values: dict[str, str | bool] = Field(
        default_factory=dict,
        description="Values keyed by declared catalog argument name",
    )


class CommandOutcome(ConsoleModel):
    """Completed CLI action and its first-level human summary."""

    result: CommandResult = Field(description="Typed public command result")
    summary: OutcomeSummary = Field(description="Concise human outcome")
    firmware_library: FirmwareLibrary | None = Field(
        default=None,
        description="Validated library data for the firmware live view",
        exclude_if=lambda value: value is None,
    )


class DeviceOutcome(ConsoleModel):
    """Completed read-only phone probe and its useful facts."""

    result: DeviceInfoResult = Field(description="Bounded device evidence")
    summary: OutcomeSummary = Field(description="Concise observed facts")


class ConsoleSnapshot(ConsoleModel):
    """Current host context, reconciled selection and status indication."""

    context: ConsoleContext = Field(
        description="Observed host and phone context"
    )
    selected_device: str | None = Field(
        default=None,
        description="Live redacted selector retained across observations",
        exclude_if=lambda value: value is None,
    )
    device_status: DeviceStatusView | None = Field(
        default=None,
        description="Selected or knowingly pending phone status",
        exclude_if=lambda value: value is None,
    )


class SyntaxSpan(ConsoleModel):
    """One safely escaped fragment of technical output."""

    category: Literal[
        "plain",
        "comment",
        "error",
        "string",
        "number",
        "keyword",
        "tag",
        "attribute",
        "operator",
    ] = Field(description="Syntax color class")
    text: str = Field(description="Exact source text fragment")
