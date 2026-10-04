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
    "device mode": "USB mode observation",
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


def _device_info(result: dict) -> str:
    """Present functions before raw interface numbers and probe details."""
    device = result["device"]
    name = " ".join(
        part
        for part in (device.get("manufacturer"), device.get("product"))
        if part
    )
    lines = [
        _style("USB device information", "1;36"),
        _style(name or "Unknown device", "1")
        + " "
        + _style(
            f"[{device['vendor_id']:04x}:{device['product_id']:04x}]",
            "2",
        ),
        f"{_style('Selector:', '36')} {device['selector']}",
        f"{_style('USB profile:', '36')} {device['interface_profile']}",
    ]
    if device.get("identity_anchor"):
        lines.append(
            f"{_style('Identity anchor:', '36')} "
            f"{device['identity_anchor']} "
            f"({_scalar(device.get('identity_basis'))})"
        )
    if device.get("capabilities"):
        lines.append(
            f"{_style('Host capabilities:', '36')} "
            + ", ".join(device["capabilities"])
        )
    interfaces = device.get("interfaces", [])
    lines.append(_style(f"USB interfaces ({len(interfaces)})", "1;36"))
    for item in interfaces:
        declared = item.get("declared_name")
        title = declared or item.get("function", "Unclassified interface")
        meaning = item.get("function") if declared else None
        codes = (
            f"{item['class_code']:02x}/{item['subclass_code']:02x}/"
            f"{item['protocol_code']:02x}"
        )
        title_color = "1;32" if item.get("host_serial_port") else "1"
        lines.append(
            "  "
            + _style(f"{item['number']:>2}", "2")
            + "  "
            + _style(title, title_color)
            + (f" — {meaning}" if meaning and meaning != title else "")
            + " "
            + _style(f"[{codes}]", "2")
        )
        details = []
        if item.get("endpoint_count") is not None:
            count = item["endpoint_count"]
            details.append(f"{count} endpoint{'s' if count != 1 else ''}")
        if item.get("alternate_setting") is not None:
            details.append(f"alternate {item['alternate_setting']}")
        if item.get("host_driver"):
            details.append(f"macOS driver {item['host_driver']}")
        if item.get("host_serial_port"):
            details.append(
                "serial port " + _style(item["host_serial_port"], "32")
            )
        if details:
            lines.append("      " + _style("; ".join(details), "2"))
    for volume in device.get("volumes", []):
        lines.append(
            _style("Storage:", "1;36")
            + f" {volume['mount']} ({volume['disk']}, "
            f"{_scalar(volume.get('filesystem'))})"
        )
    if not device.get("volumes"):
        lines.append(f"{_style('Mounted storage:', '36')} None")
    usb_map = result.get("usb_map")
    if usb_map:
        lines.append(
            f"{_style('USB function map:', '1;36')} "
            + _style(
                usb_map["state"],
                "32" if usb_map["state"] == "observed" else "33",
            )
        )
        for union in usb_map.get("cdc_unions", []):
            slaves = ", ".join(str(value) for value in union["slaves"])
            lines.append(
                f"  CDC union: master {union['master']} → subordinate {slaves}"
            )
        for interface in usb_map.get("interfaces", []):
            endpoints = interface["endpoints"]
            if not endpoints:
                continue
            path = ", ".join(
                f"{endpoint['transfer']} {endpoint['direction']} "
                f"0x{endpoint['address']:02x}"
                for endpoint in endpoints
            )
            lines.append(
                f"  Interface {interface['number']} alternate "
                f"{interface['alternate_setting']}: {path}"
            )
    mtp = result.get("mtp_probe")
    if mtp:
        lines.append(
            f"{_style('MTP probe:', '1;36')} "
            + _style(
                mtp["state"], "32" if mtp["state"] == "connected" else "33"
            )
        )
        info = mtp.get("device_info", {})
        if info:
            lines.append(
                f"  {info.get('manufacturer', '')} {info.get('model', '')} "
                f"{info.get('device_version', '')}".strip()
            )
        for storage in mtp.get("storage", []):
            lines.append(
                f"  Storage 0x{storage['id']:08x}: "
                f"{storage.get('description', storage.get('error', 'unknown'))}"
            )
            if "root_object_count" in storage:
                lines.append(
                    f"    Root objects: {storage['root_object_count']} "
                    f"(showing {len(storage.get('root_objects', []))})"
                )
                for item in storage.get("root_objects", []):
                    lines.append(
                        "      "
                        + _style(item.get("name", "unknown"), "32")
                        + f" [0x{item['handle']:08x}]"
                    )
    obex = result.get("obex_probe")
    if obex:
        lines.append(
            f"{_style('PC Suite OBEX:', '1;36')} "
            + _style(
                obex["state"], "32" if obex["state"] == "connected" else "33"
            )
        )
        if "response_code" in obex:
            lines.append(f"  Response: 0x{obex['response_code']:02x}")
        if "disconnected" in obex:
            lines.append(f"  Disconnected: {obex['disconnected']}")
    if result.get("pc_suite_usb_candidate"):
        lines.append(
            f"{_style('PC Suite USB layout:', '36')} "
            + _style("candidate", "33")
        )
    probe = result.get("protocol_probe")
    if probe:
        state_color = "32" if probe["state"] == "at-ready" else "33"
        lines.append(
            f"{_style('AT probe:', '36')} "
            + _style(probe["state"], state_color)
        )
        for key, reply in probe.get("queries", {}).items():
            if (
                key in ("manufacturer", "model", "revision")
                and result.get("reported_identity")
                and reply.get("value")
            ):
                continue
            label = "AT capabilities" if key == "capabilities" else _label(key)
            if reply.get("value"):
                lines.append(
                    f"  {_style(label + ':', '36')} " f"{reply['value']}"
                )
            else:
                lines.append(
                    f"  {_style(label + ':', '36')} "
                    + _style(reply["state"], "33")
                )
        for key, reply in probe.get("status_queries", {}).items():
            label = _label(key)
            if reply.get("value"):
                values = ", ".join(
                    f"{_label(part)} "
                    + (
                        "unavailable"
                        if key == "signal" and value == 99
                        else str(value)
                    )
                    for part, value in reply["value"].items()
                )
                lines.append(f"  {_style(label + ':', '36')} {values}")
            else:
                lines.append(
                    f"  {_style(label + ':', '36')} "
                    + _style(reply["state"], "33")
                )
    reported = result.get("reported_identity")
    if reported:
        lines.append(_style("Phone-reported identity", "1;36"))
        for key in (
            "manufacturer",
            "model",
            "firmware_revision",
            "firmware_date",
            "rm_code",
        ):
            label = "RM code" if key == "rm_code" else _label(key)
            lines.append(
                f"  {_style(label + ':', '36')} "
                f"{_scalar(reported.get(key))}"
            )
        lines.append(
            f"  {_style('Source:', '36')} {reported.get('source', 'Unknown')}"
        )
    lines.append(f"{_style('Scope:', '36')} {result['scope']}")
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
    if command == "device" and action == "info":
        return _device_info(result)
    if result is None:
        return "Done."
    title = _TITLES.get(" ".join(part for part in (command, action) if part))
    lines = _format_tree(result)
    if title:
        return f"{_style(title, '1;36')}\n" + "\n".join(lines)
    return "\n".join(lines)
