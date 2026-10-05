"""Effective form values derived from the current SDK context."""

from pathlib import Path

from symbian.console.models import CommandArgument, ConsoleContext

_PATH_DEFAULT_FIELDS = {
    "output",
    "oracles_build",
    "sources_root",
    "headers",
}


def initial_value(
    context: ConsoleContext | None,
    path: tuple[str, ...],
    argument: CommandArgument,
    selected_device: str | None,
) -> str:
    """Display actual effective paths and selected devices when known."""
    if argument.name == "device":
        return selected_device or ""
    if context is None:
        return argument.default or ""
    if argument.name == "project":
        if path == ("emu", "run"):
            return context.project or ""
        return context.project or context.workspace
    if argument.name in {"root", "workspace"}:
        return context.workspace
    if argument.name == "sdk":
        return context.sdk_manifest or ""
    if argument.name == "store":
        return context.firmware_store or ""
    if argument.default is None:
        return ""
    if argument.name in _PATH_DEFAULT_FIELDS:
        declared = Path(argument.default)
        if not declared.is_absolute():
            return str(Path(context.workspace) / declared)
    return argument.default
