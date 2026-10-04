"""Human summaries for bounded handset inspection results."""

from symbian.console.models import OutcomeFact, OutcomeSummary
from symbian.device.connection import DeviceInfoResult


def usb_class_name(code: int) -> str:
    """Name familiar USB classes while retaining the exact observed code."""
    names = {
        0x00: "Per-interface",
        0x02: "Communications",
        0x06: "Imaging",
        0x09: "Hub",
        0xEF: "Composite",
        0xFF: "Vendor-specific",
    }
    return f"{names.get(code, 'Other')} (0x{code:02x})"


def summarize_device(
    operation: str, result: DeviceInfoResult
) -> OutcomeSummary:
    """Extract useful observed facts without inferring unknown phone state."""
    facts: list[OutcomeFact] = []
    if operation == "map":
        for interface in result.device.interfaces:
            facts.append(
                OutcomeFact(
                    label=f"Interface {interface.number}",
                    value=interface.function,
                )
            )
        if not facts:
            facts.append(OutcomeFact(label="USB", value=result.usb_map.state))
    elif operation == "at-identity" and result.reported_identity:
        identity = result.reported_identity
        for label, value in (
            ("Manufacturer", identity.manufacturer),
            ("Model", identity.model),
            ("Firmware", identity.firmware_revision),
        ):
            if value:
                facts.append(OutcomeFact(label=label, value=value))
    elif operation.startswith("mtp") and result.mtp_probe:
        probe = result.mtp_probe
        facts.append(OutcomeFact(label="Probe state", value=probe.state))
        if probe.device_info:
            facts.append(
                OutcomeFact(label="Model", value=probe.device_info.model)
            )
        facts.append(
            OutcomeFact(label="Storages", value=str(len(probe.storage)))
        )
    elif operation == "obex" and result.obex_probe:
        probe = result.obex_probe
        facts.append(OutcomeFact(label="Probe state", value=probe.state))
        if probe.disconnected is not None:
            facts.append(
                OutcomeFact(
                    label="Disconnected",
                    value="Yes" if probe.disconnected else "No",
                )
            )
    elif result.protocol_probe:
        facts.append(
            OutcomeFact(label="AT modem", value=result.protocol_probe.state)
        )
    title = f"{result.device.product} · {operation.replace('-', ' ').title()}"
    return OutcomeSummary(
        title=title,
        message=result.scope,
        facts=tuple(facts),
    )
