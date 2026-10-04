"""Typed messages exchanged by the console frontend and in-process API."""

from typing import Any, Literal

from pydantic import BaseModel, ConfigDict, Field

from symbian.device.connection import ConnectedDevice


class ConsoleModel(BaseModel):
    """Immutable console message with no undeclared fields."""

    model_config = ConfigDict(extra="forbid", frozen=True)


class CommandArgument(ConsoleModel):
    """One CLI argument available in a generated workflow form."""

    name: str = Field(description="Argparse destination name")
    label: str = Field(description="Human-readable argument label")
    flags: tuple[str, ...] = Field(
        default=(),
        description="Accepted option flags",
        exclude_if=lambda value: not value,
    )
    help: str = Field(
        default="",
        description="CLI argument guidance",
        exclude_if=lambda value: not value,
    )
    required: bool = Field(
        default=False,
        description="Input must be supplied",
        exclude_if=lambda value: not value,
    )
    kind: Literal["text", "flag", "choice"] = Field(
        default="text", description="Input control type"
    )
    choices: tuple[str, ...] = Field(
        default=(),
        description="Allowed named values",
        exclude_if=lambda value: not value,
    )
    repeatable: bool = Field(
        default=False,
        description="Option accepts multiple values",
        exclude_if=lambda value: not value,
    )
    positional: bool = Field(
        default=False,
        description="Argument has no option flag",
        exclude_if=lambda value: not value,
    )
    default: str | None = Field(
        default=None,
        description="Meaningful displayed default",
        exclude_if=lambda value: value is None,
    )


class CommandSpec(ConsoleModel):
    """One executable public CLI workflow."""

    path: tuple[str, ...] = Field(description="CLI command path")
    title: str = Field(description="Displayed workflow name")
    category: str = Field(description="Top-level workflow category")
    description: str = Field(description="CLI command description")
    arguments: tuple[CommandArgument, ...] = Field(
        default=(), description="User-settable arguments"
    )


class CommandCatalog(ConsoleModel):
    """Generated view of every runnable CLI workflow."""

    schema_name: Literal["symbian.console-catalog/v1"] = Field(
        default="symbian.console-catalog/v1", description="Catalog schema"
    )
    commands: tuple[CommandSpec, ...] = Field(description="Runnable workflows")


class ConsoleContext(ConsoleModel):
    """Actual host defaults and safely observed phones for the GUI."""

    workspace: str = Field(description="Current host working directory")
    project: str | None = Field(
        default=None,
        description="Application project detected in the working directory",
        exclude_if=lambda value: value is None,
    )
    sdk_manifest: str | None = Field(
        default=None,
        description="Currently selected installed SDK manifest",
        exclude_if=lambda value: value is None,
    )
    firmware_store: str | None = Field(
        default=None,
        description="Resolved local firmware content store",
        exclude_if=lambda value: value is None,
    )
    devices: tuple[ConnectedDevice, ...] = Field(
        default=(),
        description="Connected supported phones without serial strings",
        exclude_if=lambda value: not value,
    )


class CommandRequest(ConsoleModel):
    """CLI workflow invocation sent through the in-memory transport."""

    path: tuple[str, ...] = Field(description="Selected catalog command path")
    argv: tuple[str, ...] = Field(
        default=(), description="Argument tokens for the selected command"
    )
    cwd: str | None = Field(
        default=None,
        description="Selected working directory for the isolated CLI child",
        exclude_if=lambda value: value is None,
    )
    sdk_manifest: str | None = Field(
        default=None,
        description="Selected SDK manifest for child discovery",
        exclude_if=lambda value: value is None,
    )
    build_log_path: str | None = Field(
        default=None,
        description="Private live tool output log for build or emulator run",
        exclude_if=lambda value: value is None,
    )
    run_session_path: str | None = Field(
        default=None,
        description="Private file receiving the active emulator session path",
        exclude_if=lambda value: value is None,
    )


class CommandResult(ConsoleModel):
    """Result of one SDK CLI workflow."""

    path: tuple[str, ...] = Field(description="Executed command path")
    result: Any | None = Field(
        default=None,
        description="SDK result data",
        exclude_if=lambda value: value is None,
    )


class DeviceInspectRequest(ConsoleModel):
    """One bounded USB or protocol inspection request."""

    selector: str | None = Field(
        default=None,
        description="Exact device selector; omit only for one device",
        exclude_if=lambda value: value is None,
    )
    operation: Literal[
        "map", "at-identity", "at-status", "mtp", "mtp-list", "obex"
    ] = Field(default="map", description="Read-only protocol operation")
    limit: int = Field(
        default=8,
        ge=0,
        le=128,
        description="Maximum MTP root handles per storage",
        exclude_if=lambda value: value == 8,
    )


class ActivityEntry(ConsoleModel):
    """One completed request in the local GUI session."""

    time: str = Field(description="Local completion time")
    action: str = Field(description="User-facing operation name")
    outcome: str = Field(description="Result or error summary")


class OutcomeFact(ConsoleModel):
    """One concise fact in a completed task summary."""

    label: str = Field(description="User-facing fact name")
    value: str = Field(description="Observed or generated value")


class OutcomeSummary(ConsoleModel):
    """First-level result shown before optional technical details."""

    title: str = Field(description="Outcome headline")
    message: str = Field(description="Short next-step or scope explanation")
    facts: tuple[OutcomeFact, ...] = Field(
        default=(),
        description="Most useful result facts",
        exclude_if=lambda value: not value,
    )
