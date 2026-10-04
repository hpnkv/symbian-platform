"""Bounded, explicit device protocol probes with concise outcome summaries."""

import tkinter as tk
from collections.abc import Callable
from tkinter import ttk
from typing import Any

from symbian.console.client import ConsoleClient
from symbian.console.models import (
    DeviceInspectRequest,
    OutcomeFact,
    OutcomeSummary,
)
from symbian.console.selection import device_labels
from symbian.console.widgets import OutcomePanel, title
from symbian.device.connection import ConnectedDevice, DeviceInfoResult
from symbian.status import Status

Submit = Callable[
    [Callable[[], Any], Callable[[Any, Exception | None], None], str], None
]


class ProtocolPanel(ttk.Frame):
    """Group AT, MTP and OBEX communication by protocol and purpose."""

    def __init__(
        self,
        parent: tk.Misc,
        client: ConsoleClient,
        submit: Submit,
        on_device_selected: Callable[[str], None] | None = None,
        refresh_context: Callable[[], None] | None = None,
    ) -> None:
        super().__init__(parent, padding=18)
        self._client = client
        self._submit = submit
        self._device_labels: dict[str, str] = {}
        self._on_device_selected = on_device_selected
        self._refresh_context = refresh_context
        self._result_selector: str | None = None
        self._result_cache: dict[tuple[str, str], DeviceInfoResult] = {}
        self._last_operation: dict[str, str] = {}
        title(
            self,
            "Device protocols",
            "Select a phone, then choose the information you need.",
        ).pack(fill="x", pady=(0, 16))
        controls = ttk.Frame(self)
        controls.pack(fill="x", pady=(0, 15))
        ttk.Label(controls, text="Phone").pack(side="left")
        self.selector = tk.StringVar()
        self.device_combo = ttk.Combobox(
            controls,
            textvariable=self.selector,
            state="readonly",
            width=42,
        )
        self.device_combo.pack(side="left", padx=9)
        self.device_combo.bind(
            "<<ComboboxSelected>>", lambda _event: self._notify_device()
        )
        ttk.Button(controls, text="Refresh", command=self.refresh).pack(
            side="left"
        )
        self.limit = tk.StringVar(value="8")
        at_section = ttk.LabelFrame(self, text="AT modem", padding=11)
        at_section.pack(fill="x", pady=(0, 8))
        ttk.Label(
            at_section,
            text="Read phone identity, battery and signal codes.",
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(0, 8))
        at_actions = ttk.Frame(at_section)
        at_actions.pack(anchor="w")
        ttk.Button(
            at_actions,
            text="Read identity",
            command=lambda: self._run("at-identity"),
        ).pack(side="left")
        ttk.Button(
            at_actions,
            text="Read status",
            command=lambda: self._run("at-status"),
        ).pack(side="left", padx=(8, 0))

        mtp_section = ttk.LabelFrame(self, text="MTP / PTP", padding=11)
        mtp_section.pack(fill="x", pady=(0, 8))
        ttk.Label(
            mtp_section,
            text="Read metadata or a bounded root object listing.",
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(0, 8))
        mtp_actions = ttk.Frame(mtp_section)
        mtp_actions.pack(anchor="w")
        ttk.Button(
            mtp_actions,
            text="Read metadata",
            command=lambda: self._run("mtp"),
        ).pack(side="left")
        ttk.Button(
            mtp_actions,
            text="List root objects",
            command=lambda: self._run("mtp-list"),
        ).pack(side="left", padx=(8, 12))
        ttk.Label(mtp_actions, text="Items per storage").pack(side="left")
        ttk.Spinbox(
            mtp_actions,
            from_=0,
            to=128,
            textvariable=self.limit,
            width=5,
        ).pack(side="left", padx=(7, 0))

        obex_section = ttk.LabelFrame(self, text="PC Suite OBEX", padding=11)
        obex_section.pack(fill="x")
        ttk.Label(
            obex_section,
            text="Try Connect/Disconnect without browsing or transfer.",
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(0, 8))
        ttk.Button(
            obex_section,
            text="Connect and disconnect",
            command=lambda: self._run("obex"),
        ).pack(anchor="w")
        self.status = ttk.Label(self, text="", style="Muted.TLabel")
        self.status.pack(anchor="w", pady=(9, 0))
        self.outcome = OutcomePanel(self)

    def refresh(self) -> None:
        """Refresh supported device selectors."""
        self.status.configure(text="Finding phones…")
        if self._refresh_context is not None:
            self._refresh_context()

    def set_devices(
        self, devices: tuple[ConnectedDevice, ...], selected: str | None
    ) -> None:
        """Reconcile selectors and remove results for a disconnected phone."""
        self._device_labels = device_labels(devices)
        labels = tuple(self._device_labels)
        self.device_combo.configure(values=labels)
        if (
            self._result_selector
            and self._result_selector not in self._device_labels.values()
        ):
            self.outcome.hide()
            self._result_selector = None
        self.select_device(selected)
        self.status.configure(text=f"{len(labels)} supported phones")

    def select_device(self, selector: str | None) -> None:
        """Follow the phone selected in another view."""
        previous = self._device_labels.get(self.selector.get())
        if previous == selector:
            return
        if (
            self._result_selector is not None
            and self._result_selector != selector
        ):
            self.outcome.hide()
            self._result_selector = None
        self.selector.set("")
        for label, candidate in self._device_labels.items():
            if candidate == selector:
                self.selector.set(label)
                break
        if selector is not None:
            operation = self._last_operation.get(selector)
            cached = (
                self._result_cache.get((selector, operation))
                if operation is not None
                else None
            )
            if cached is not None and operation is not None:
                self._result_selector = selector
                self.outcome.show(
                    summarize_protocol(operation, cached),
                    cached.model_dump(mode="json", by_alias=True),
                )
                self.status.configure(
                    text="Previous result · run a probe to refresh"
                )

    def _notify_device(self) -> None:
        selector = self._device_labels.get(self.selector.get())
        if selector and self._on_device_selected is not None:
            self._on_device_selected(selector)

    def _run(self, operation: str) -> None:
        selector = self._device_labels.get(self.selector.get())
        if not selector:
            self.status.configure(text="Select a phone first.")
            return
        try:
            limit = int(self.limit.get()) if operation == "mtp-list" else 8
            request = DeviceInspectRequest(
                selector=selector, operation=operation, limit=limit
            )
        except (ValueError, TypeError) as error:
            self.status.configure(text=f"Invalid MTP limit: {error}")
            return
        self.status.configure(
            text=(
                f"Running {operation}… · previous result remains visible"
                if self._result_selector == selector
                else f"Running {operation}…"
            )
        )
        self._result_selector = selector
        self._submit(
            lambda: self._client.inspect(request),
            lambda value, error: self._completed(operation, value, error),
            f"Probe {operation}",
        )

    def _completed(
        self, operation: str, value: Any, error: Exception | None
    ) -> None:
        if error is not None:
            status = Status.from_exception(error)
            self.status.configure(text="Protocol probe failed")
            self.outcome.show(
                OutcomeSummary(
                    title="Probe failed",
                    message=f"{status.code.name}: {status.message}",
                ),
                status.model_dump(mode="json"),
            )
            return
        result: DeviceInfoResult = value
        self._result_cache[(result.device.selector, operation)] = result
        self._last_operation[result.device.selector] = operation
        if result.device.selector not in self._device_labels.values():
            self.status.configure(text="Phone disconnected during probe")
            return
        if result.device.selector != self._device_labels.get(
            self.selector.get()
        ):
            self.status.configure(text="Phone selection changed during probe")
            return
        self.status.configure(text=result.device.product)
        self.outcome.show(
            summarize_protocol(operation, result),
            result.model_dump(mode="json", by_alias=True),
        )


def summarize_protocol(
    operation: str, result: DeviceInfoResult
) -> OutcomeSummary:
    """Present observed protocol evidence without guessing phone state."""
    if operation in {"at-identity", "at-status"}:
        probe = result.protocol_probe
        facts = [
            OutcomeFact(
                label="Probe state",
                value=probe.state if probe else "Unavailable",
            )
        ]
        identity = result.reported_identity
        if identity is not None:
            for label, value in (
                ("Manufacturer", identity.manufacturer),
                ("Model", identity.model),
                ("Firmware revision", identity.firmware_revision),
            ):
                if value:
                    facts.append(OutcomeFact(label=label, value=value))
        if probe and probe.status_queries:
            battery = probe.status_queries.battery.value
            if battery:
                facts.append(
                    OutcomeFact(
                        label="Battery charge",
                        value=f"{battery.charge_percent}%",
                    )
                )
        return OutcomeSummary(
            title="AT modem response",
            message="Values are reported by the phone's AT interface.",
            facts=tuple(facts),
        )
    if operation in {"mtp", "mtp-list"}:
        probe = result.mtp_probe
        facts = [
            OutcomeFact(
                label="Probe state",
                value=probe.state if probe else "Unavailable",
            )
        ]
        if probe and probe.device_info:
            facts.append(
                OutcomeFact(label="Device", value=probe.device_info.model)
            )
        if probe and probe.storage_count is not None:
            facts.append(
                OutcomeFact(label="Storages", value=str(probe.storage_count))
            )
        return OutcomeSummary(
            title="MTP / PTP response",
            message="Metadata and bounded root listings are shown in details.",
            facts=tuple(facts),
        )
    probe = result.obex_probe
    return OutcomeSummary(
        title="PC Suite OBEX response",
        message="Connect/Disconnect evidence only; no browsing was attempted.",
        facts=(
            OutcomeFact(
                label="Probe state",
                value=probe.state if probe else "Unavailable",
            ),
            OutcomeFact(
                label="Disconnected",
                value=("Yes" if probe and probe.disconnected else "Unknown"),
            ),
        ),
    )
