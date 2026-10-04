"""Fast, local validation for task wizard inputs."""

import re
import shlex
from collections.abc import Mapping

from pydantic import Field

from symbian.console.models import CommandArgument, CommandSpec, ConsoleModel
from symbian.console.presentation import LABELS


class ValidationIssue(ConsoleModel):
    """Actionable input problem shown beside a wizard field."""

    field: str = Field(description="CLI argument destination")
    message: str = Field(description="Correction needed before continuing")


def field_label(path: tuple[str, ...], argument: CommandArgument) -> str:
    """Choose a task-specific label instead of a command-line flag."""
    if path == ("init",) and argument.name == "destination":
        return "New application folder"
    if path == ("sdk", "install") and argument.name == "destination":
        return "SDK destination folder"
    if path == ("firmware", "export") and argument.name == "destination":
        return "Bundle destination"
    if path == ("device", "mode", "begin") and argument.name == "ticket":
        return "New private ticket file"
    if path == ("device", "mode", "verify") and argument.name == "ticket":
        return "Saved ticket file"
    return LABELS.get(argument.name, argument.label)


def validate_field(
    argument: CommandArgument, value: str | bool
) -> ValidationIssue | None:
    """Check one value without filesystem or device I/O on the GUI thread."""
    if argument.kind == "flag":
        return None
    entered = str(value).strip()
    if not entered:
        if argument.required:
            return ValidationIssue(
                field=argument.name, message="This field is required."
            )
        return None
    if "\x00" in entered:
        return ValidationIssue(
            field=argument.name, message="NUL characters are not allowed."
        )
    if argument.choices and entered not in argument.choices:
        return ValidationIssue(
            field=argument.name,
            message="Choose one of the listed values.",
        )
    if argument.repeatable:
        try:
            if not shlex.split(entered):
                raise ValueError("No values supplied")
        except ValueError:
            return ValidationIssue(
                field=argument.name,
                message="Separate values with spaces; close all quotes.",
            )
    if argument.name == "uid3":
        try:
            parsed = int(entered, 0)
        except ValueError:
            parsed = -1
        if parsed < 0 or parsed > 0xFFFFFFFF:
            return ValidationIssue(
                field=argument.name,
                message="Enter a 32-bit UID in decimal or 0x hexadecimal.",
            )
    if argument.name in {"x", "y", "language", "mtp_list", "variant"}:
        try:
            parsed = int(entered)
        except ValueError:
            parsed = -2
        minimum = -1 if argument.name == "variant" else 0
        maximum = 128 if argument.name == "mtp_list" else None
        if parsed < minimum or (maximum is not None and parsed > maximum):
            limit = f" to {maximum}" if maximum is not None else ""
            return ValidationIssue(
                field=argument.name,
                message=f"Enter a whole number from {minimum}{limit}.",
            )
    if argument.name == "timeout":
        try:
            parsed_float = float(entered)
        except ValueError:
            parsed_float = 0
        if not 0 < parsed_float <= 3600:
            return ValidationIssue(
                field=argument.name,
                message="Enter a timeout above 0 and at most 3600 seconds.",
            )
    if argument.name == "manifest_sha256" and not re.fullmatch(
        r"[0-9a-fA-F]{64}", entered
    ):
        return ValidationIssue(
            field=argument.name,
            message="Enter the 64-character SHA-256 digest.",
        )
    return None


def validate_task(
    specification: CommandSpec, values: Mapping[str, str | bool]
) -> tuple[ValidationIssue, ...]:
    """Validate a complete task including cross-field requirements."""
    issues = [
        issue
        for argument in specification.arguments
        if (issue := validate_field(argument, values.get(argument.name, "")))
        is not None
    ]
    if specification.path == ("firmware", "import"):
        sources = ("source", "rom", "vpl", "instance", "bundle")
        supplied = [
            name for name in sources if str(values.get(name, "")).strip()
        ]
        if len(supplied) != 1:
            issues.append(
                ValidationIssue(
                    field="source",
                    message=(
                        "Choose exactly one source: archive, ROM, VPL, "
                        "instance or bundle."
                    ),
                )
            )
        if values.get("rpkg") or values.get("z_drive"):
            if not values.get("rom"):
                issues.append(
                    ValidationIssue(
                        field="rom",
                        message="RPKG and drive-Z companions need a ROM image.",
                    )
                )
        if values.get("rpkg") and values.get("z_drive"):
            issues.append(
                ValidationIssue(
                    field="rpkg",
                    message="Choose either RPKG or drive-Z, not both.",
                )
            )
    return tuple(issues)
