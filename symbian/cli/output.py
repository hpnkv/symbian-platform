"""Human and machine-readable presentation of CLI results."""

import json
import os
import sys
from typing import Any

_TITLES = {
    "doctor": "Host environment",
    "init": "Application project created",
    "inspect": "Artifact inspection",
    "build": "Application build",
    "package": "SIS package",
    "device list": "Connected devices",
    "device info": "USB device information",
    "device policy": "Device operation policy",
    "device install": "SIS staged for on-device installation",
    "firmware list": "Imported firmware",
    "firmware inspect": "Firmware information",
    "firmware probe": "Firmware archive probe",
    "emu resolve": "Emulator configuration",
    "emu status": "Emulator status",
    "sdk install": "SDK installed",
}


def _style(value: str, code: str) -> str:
    if (
        not sys.stdout.isatty()
        or "NO_COLOR" in os.environ
        or os.environ.get("TERM") == "dumb"
    ):
        return value
    return f"\x1b[{code}m{value}\x1b[0m"


def _bytes(value: int) -> str:
    amount = float(value)
    for unit in ("B", "KiB", "MiB", "GiB", "TiB"):
        if abs(amount) < 1024 or unit == "TiB":
            if unit == "B":
                return f"{value:,} B"
            return f"{amount:,.1f} {unit} ({value:,} bytes)"
        amount /= 1024
    return f"{value:,} B"


def _label(key: str | int) -> str:
    text = str(key).replace("_", " ").replace("-", " ")
    return text[0].upper() + text[1:] if text else text


def _scalar(value: Any, key: str = "") -> str:
    if value is None:
        return "Unknown"
    if isinstance(value, bool):
        return "Yes" if value else "No"
    if key.endswith("_bytes") and isinstance(value, int):
        return _bytes(value)
    return str(value)


def _format_tree(value: Any, indent: int = 0) -> list[str]:
    spaces = " " * indent
    if isinstance(value, dict):
        lines = []
        labels = [_label(key) for key in value if key != "schema"]
        width = min(max(map(len, labels), default=0), 26)
        for key, item in value.items():
            if key == "schema":
                continue
            label = _label(key)
            if isinstance(item, (dict, list)):
                lines.append(f"{spaces}{_style(label, '1;36')}:")
                lines.extend(_format_tree(item, indent + 2))
            else:
                padded = f"{label}:".ljust(width + 1)
                lines.append(
                    f"{spaces}{_style(padded, '36')} {_scalar(item, str(key))}"
                )
        return lines
    if isinstance(value, list):
        if not value:
            return [f"{spaces}None"]
        if all(not isinstance(item, (dict, list)) for item in value):
            joined = ", ".join(_scalar(item) for item in value)
            if len(joined) <= 76 and len(value) <= 3:
                return [f"{spaces}{joined}"]
            return [
                f"{spaces}{_style(f'{index}.', '1;36')} {_scalar(item)}"
                for index, item in enumerate(value, 1)
            ]
        lines = []
        for index, item in enumerate(value, 1):
            lines.append(f"{spaces}{_style(f'{index}.', '1;36')}")
            lines.extend(_format_tree(item, indent + 2))
        return lines
    return [f"{spaces}{_scalar(value)}"]


def _device_list(result: dict) -> str:
    devices = result.get("devices", [])
    if not devices:
        return "No connected Symbian devices found."
    lines = [f"{_style('Connected devices', '1;36')}: {len(devices)}"]
    for index, device in enumerate(devices, 1):
        name = (
            " ".join(
                part
                for part in (device.get("manufacturer"), device.get("product"))
                if part
            )
            or "Unknown device"
        )
        lines.append(f"\n{_style(f'{index}. {name}', '1')}")
        lines.append(f"   Selector: {_scalar(device.get('selector'))}")
        lines.append(f"   Transport: {_scalar(device.get('transport'))}")
        capabilities = ", ".join(device.get("capabilities", [])) or "none"
        lines.append(f"   Capabilities: {capabilities}")
        for volume in device.get("volumes", []):
            lines.append(
                f"   Storage: {_scalar(volume.get('mount'))} "
                f"({volume.get('disk', 'unknown disk')}, "
                f"{_scalar(volume.get('filesystem'))})"
            )
            free = volume.get("free_bytes")
            free_text = _bytes(free) if isinstance(free, int) else "Unknown"
            lines.append(
                f"     Free: {free_text}; "
                f"SIS staging: {_scalar(volume.get('stage_sis'))}"
            )
    return "\n".join(lines)


def render(
    response: dict, output_format: str, command: str, action: str = ""
) -> str:
    """Renders one canonical response without changing its data or schema.

    Args:
        response: Canonical CLI response dictionary.
        output_format: ``human`` or ``json``.
        command: Top-level CLI command.
        action: Selected nested command, if any.

    Returns:
        Text to print to the CLI output stream.
    """
    if output_format == "json":
        return json.dumps(response, sort_keys=True, ensure_ascii=True)
    status = response["status"]
    if status["code"] != 0:
        return f"Error ({status['name']}): {status['message']}"
    result = response.get("result")
    if command == "device" and action == "list":
        return _device_list(result)
    if result is None:
        return "Done."
    title = _TITLES.get(" ".join(part for part in (command, action) if part))
    lines = _format_tree(result)
    if title:
        return f"{_style(title, '1;36')}\n" + "\n".join(lines)
    return "\n".join(lines)
