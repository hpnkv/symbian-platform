"""Local, user-reported phone agent state; no claim of a live USB session."""

import json
import os
import tempfile
from datetime import datetime, timezone
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict, Field, ValidationError

from symbian.device.connection import ConnectedDevice
from symbian.paths import asset_directory
from symbian.status import Code, StatusError


class AgentObservation(BaseModel):
    """A human report tied to a serial-derived identity anchor."""

    model_config = ConfigDict(extra="forbid", frozen=True)

    identity_anchor: str = Field(description="Hashed USB serial anchor")
    reported_at: str = Field(description="UTC time of the user report")
    state: Literal["running-reported"] = Field(
        default="running-reported", description="Evidence class"
    )


def default_path() -> Path:
    """Return the user-local observation file, outside project sources."""
    return asset_directory("data") / "agent-observations.json"


def _read(path: Path) -> dict[str, AgentObservation]:
    if not path.exists():
        return {}
    try:
        raw = path.read_bytes()
        if len(raw) > 64 * 1024:
            raise ValueError("Observation file exceeds 64 KiB")
        document = json.loads(raw)
        if document.get("schema") != "symbian.agent-observations/v1":
            raise ValueError("Unknown observation schema")
        observations = {
            key: AgentObservation.model_validate(value)
            for key, value in document["devices"].items()
        }
        if any(
            key != value.identity_anchor for key, value in observations.items()
        ):
            raise ValueError("Observation anchor mismatch")
        return observations
    except (
        OSError,
        ValueError,
        TypeError,
        KeyError,
        AttributeError,
        ValidationError,
    ) as error:
        raise StatusError(
            Code.DATA_LOSS, "Invalid agent observations"
        ) from error


def _write(path: Path, observations: dict[str, AgentObservation]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    document = {
        "schema": "symbian.agent-observations/v1",
        "devices": {
            key: value.model_dump(mode="json")
            for key, value in sorted(observations.items())
        },
    }
    temporary = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="w", encoding="utf-8", dir=path.parent, delete=False
        ) as stream:
            temporary = Path(stream.name)
            json.dump(document, stream, sort_keys=True, indent=2)
            stream.write("\n")
            stream.flush()
            os.fsync(stream.fileno())
        temporary.replace(path)
    finally:
        if temporary is not None:
            temporary.unlink(missing_ok=True)


def _anchor(device: ConnectedDevice) -> str:
    anchor = device.identity_anchor
    if (
        device.identity_basis != "usb-serial"
        or len(anchor) != 24
        or any(character not in "0123456789abcdef" for character in anchor)
    ):
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "A serial-derived device identity is required",
        )
    return anchor


def read_for_devices(
    devices: tuple[ConnectedDevice, ...], path: Path | None = None
) -> dict[str, dict]:
    """Return user reports keyed by current selector, never as live status."""
    observations = _read(path or default_path())
    return {
        device.selector: observations[device.identity_anchor].model_dump(
            mode="json"
        )
        for device in devices
        if device.identity_basis == "usb-serial"
        and device.identity_anchor in observations
    }


def report_running(
    device: ConnectedDevice, path: Path | None = None
) -> AgentObservation:
    """Record that a person reports seeing the agent running on this phone."""
    destination = path or default_path()
    observations = _read(destination)
    anchor = _anchor(device)
    report = AgentObservation(
        identity_anchor=anchor,
        reported_at=datetime.now(timezone.utc).isoformat(timespec="seconds"),
    )
    observations[anchor] = report
    _write(destination, observations)
    return report


def clear_report(device: ConnectedDevice, path: Path | None = None) -> None:
    """Remove a stale user report without touching the phone."""
    destination = path or default_path()
    observations = _read(destination)
    observations.pop(_anchor(device), None)
    _write(destination, observations)
