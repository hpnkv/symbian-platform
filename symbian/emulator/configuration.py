"""Portable emulator preferences with explicit, inspectable precedence."""

import json
import os
import tempfile
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict, ValidationError

from symbian.status import Code, StatusError


def xdg(kind: str) -> Path:
    """Returns the absolute XDG directory, with XDG defaults."""
    defaults = {"CONFIG": ".config", "DATA": ".local/share", "CACHE": ".cache"}
    value = os.environ.get(f"XDG_{kind}_HOME")
    if value and not Path(value).is_absolute():
        raise StatusError(
            Code.INVALID_ARGUMENT, f"XDG_{kind}_HOME must be absolute"
        )
    return Path(value) if value else Path.home() / defaults[kind]


class Settings(BaseModel):
    """Missing keys inherit; null explicitly clears an inherited selection."""

    model_config = ConfigDict(extra="forbid")

    firmware: str | None = None
    store: Path | None = None
    emulator: Path | None = None
    importer: Path | None = None
    backend: Literal["dynarmic", "dyncom"] | None = None
    language: int | None = None
    profile: Literal["auto", "default", "rm807-113.010.1508"] | None = None


class Resolution(BaseModel):
    """Resolved values and the file or command supplying each value."""

    settings: Settings
    origins: dict[str, str]
    layers: list[dict]
    sdk: Path | None = None
    legacy_instance: Path | None = None


def atomic_json(path: Path, value: dict) -> None:
    """Publishes a complete JSON file in one filesystem rename."""
    path.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.NamedTemporaryFile(
        mode="w", dir=path.parent, delete=False
    ) as stream:
        temporary = Path(stream.name)
        try:
            json.dump(value, stream, indent=2, sort_keys=True)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        except BaseException:
            temporary.unlink(missing_ok=True)
            raise
    try:
        temporary.replace(path)
    finally:
        temporary.unlink(missing_ok=True)


def read_settings(path: Path) -> Settings:
    """Rejects malformed files rather than silently falling back."""
    try:
        return Settings.model_validate_json(path.read_text())
    except (ValidationError, UnicodeError) as error:
        raise StatusError(Code.INVALID_ARGUMENT, f"{path}: {error}") from error


def sdk_prefix(project: Path | None, sdk: Path | None) -> Path | None:
    """Uses the explicit SDK, project selector, then active SDK."""
    if sdk is not None:
        return sdk.resolve() if sdk.is_dir() else sdk.resolve().parent
    if project is not None and (project / "sdk-location.json").is_file():
        from symbian.project.configuration import ProjectConfiguration

        if (project / "symbian-project.json").is_file():
            location = ProjectConfiguration.load(project).sdk_location
        else:
            declared = Path(
                json.loads((project / "sdk-location.json").read_text())["sdk"]
            )
            location = (
                declared
                if declared.is_absolute()
                else (project / declared).resolve()
            )
        return location
    from symbian.project.sdk import discover_sdk

    try:
        return discover_sdk(None).resolve().parent
    except StatusError as error:
        if error.code not in (Code.NOT_FOUND, Code.FAILED_PRECONDITION):
            raise
        return None


def config_path(
    scope: str, *, project: Path | None = None, sdk: Path | None = None
) -> Path:
    """Finds a scope without writing SDK/project artifacts implicitly."""
    if scope == "global":
        return xdg("CONFIG") / "symbian/emulator.json"
    if scope == "project":
        if project is None:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Project scope requires --project"
            )
        return project.resolve() / "emulator.json"
    if scope == "sdk":
        prefix = sdk_prefix(project, sdk)
        if prefix is None:
            raise StatusError(
                Code.NOT_FOUND, "SDK scope requires an installed SDK"
            )
        return prefix / "emulator.json"
    raise StatusError(Code.INVALID_ARGUMENT, "Unknown configuration scope")


def resolve(
    *,
    project: Path | None = None,
    sdk: Path | None = None,
    overrides: dict | None = None,
    root: Path | None = None,
) -> Resolution:
    """Merges global, SDK, project and command keys, retaining their origins."""
    prefix = sdk_prefix(project, sdk)
    values = {
        "store": xdg("DATA") / "symbian/firmware",
        "backend": "dynarmic",
        "language": 1,
        "profile": "auto",
    }
    origins = {key: "default" for key in values}
    legacy = None
    # Host-tool defaults are declarations, not implicit firmware selections.
    if prefix is not None and (prefix / "sdk.json").is_file():
        from symbian.project.sdk import AppSdk

        try:
            declaration = AppSdk.model_validate_json(
                (prefix / "sdk.json").read_text()
            )
        except ValidationError as error:
            raise StatusError(
                Code.INVALID_ARGUMENT, f"{prefix / 'sdk.json'}: {error}"
            ) from error
        values.update(
            emulator=declaration.emulator,
            importer=declaration.firmware_importer,
        )
        origins.update(
            emulator=str(prefix / "sdk.json"), importer=str(prefix / "sdk.json")
        )
        legacy = declaration.golden
    elif root is not None:
        values.update(
            emulator=root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1",
            importer=root
            / "build/eka2l1/platform-control/symbian_firmware_tool",
        )
        origins.update(
            emulator="workspace tool default", importer="workspace tool default"
        )
    paths = [config_path("global")]
    if prefix is not None:
        paths.append(prefix / "emulator.json")
    if project is not None:
        paths.append(project.resolve() / "emulator.json")
    layers = []
    for path in paths:
        layer = {"path": str(path), "exists": path.is_file()}
        if path.is_file():
            supplied = read_settings(path).model_dump(exclude_unset=True)
            layer["settings"] = {
                key: str(value) if isinstance(value, Path) else value
                for key, value in supplied.items()
            }
            for key, value in supplied.items():
                if isinstance(value, Path):
                    value = (path.parent / value).resolve()
                values[key], origins[key] = value, str(path)
        layers.append(layer)
    try:
        command = Settings.model_validate(overrides or {}).model_dump(
            exclude_unset=True
        )
        for key, value in command.items():
            values[key] = value.resolve() if isinstance(value, Path) else value
            origins[key] = "command"
        settings = Settings.model_validate(values)
    except ValidationError as error:
        raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
    # An explicit null disables even the deprecated fallback.
    if "firmware" in origins:
        legacy = None
    return Resolution(
        settings=settings,
        origins=origins,
        layers=layers,
        sdk=prefix,
        legacy_instance=legacy,
    )


def configure(
    scope: str,
    values: dict,
    *,
    unset: list[str] | None = None,
    project: Path | None = None,
    sdk: Path | None = None,
) -> dict:
    """Updates emulator preferences without regenerating application files."""
    path = config_path(scope, project=project, sdk=sdk)
    existing = (
        read_settings(path).model_dump(mode="json", exclude_unset=True)
        if path.is_file()
        else {}
    )
    existing.update(values)
    for key in unset or []:
        if key not in Settings.model_fields:
            raise StatusError(Code.INVALID_ARGUMENT, f"Unknown setting: {key}")
        existing.pop(key, None)
    try:
        Settings.model_validate(existing)
    except ValidationError as error:
        raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
    atomic_json(path, existing)
    return {"path": str(path), "settings": existing}


def add_options(parser, *, firmware: bool = True) -> None:
    """Shares exactly the same override flags between CLI, Run and Debug."""
    if firmware:
        parser.add_argument(
            "--firmware", help="Local alias or portable sha256:ID"
        )
    for key in ("store", "emulator", "importer"):
        parser.add_argument(f"--{key}", type=Path)
    parser.add_argument("--backend", choices=("dynarmic", "dyncom"))
    parser.add_argument("--language", type=int)
    parser.add_argument(
        "--profile", choices=("auto", "default", "rm807-113.010.1508")
    )


def options(args) -> dict:
    """Omits unspecified flags so they do not erase inherited settings."""
    return {
        key: getattr(args, key)
        for key in Settings.model_fields
        if getattr(args, key, None) is not None
    }


def option_arguments(args) -> list[str]:
    """Preserves overrides when dispatching into a project-selected SDK."""
    return [
        item
        for key, value in options(args).items()
        for item in (f"--{key}", str(value))
    ]
