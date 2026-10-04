"""USB inventory and descriptor inspection panel."""

import tkinter as tk
from collections.abc import Callable
from tkinter import ttk
from typing import Any

from symbian.console.client import ConsoleClient
from symbian.console.device_presentation import usb_class_name
from symbian.console.models import DeviceInspectRequest
from symbian.console.selection import device_labels
from symbian.console.widgets import (
    SyntaxViewer,
    configure_tree_rows,
    title,
)
from symbian.device.connection import ConnectedDevice, DeviceInfoResult
from symbian.device.usb_models import UsbDeviceDescriptor

Submit = Callable[
    [Callable[[], Any], Callable[[Any, Exception | None], None], str], None
]


class UsbInspectorPanel(ttk.Frame):
    """Show all host USB devices and a deeper supported-device map."""

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
        self._descriptors: tuple[UsbDeviceDescriptor, ...] = ()
        self._selectors: tuple[str, ...] = ()
        self._device_labels: dict[str, str] = {}
        self._on_device_selected = on_device_selected
        self._refresh_context = refresh_context
        self._inspected: DeviceInfoResult | None = None
        self._inspection_cache: dict[str, DeviceInfoResult] = {}
        self._inspection_selector: str | None = None
        title(
            self,
            "USB inspector",
            "Explore host descriptors and Symbian interfaces.",
        ).pack(fill="x", pady=(0, 15))
        controls = ttk.Frame(self)
        controls.pack(fill="x", pady=(0, 12))
        ttk.Button(
            controls,
            text="Refresh USB inventory",
            style="Accent.TButton",
            command=self.refresh,
        ).pack(side="left")
        self.selector = tk.StringVar()
        self.device_combo = ttk.Combobox(
            controls,
            textvariable=self.selector,
            state="readonly",
            width=34,
        )
        self.device_combo.pack(side="left", padx=10)
        self.device_combo.bind(
            "<<ComboboxSelected>>", lambda _event: self._notify_device()
        )
        ttk.Button(
            controls, text="Inspect selected device", command=self.inspect
        ).pack(side="left")
        self.status = ttk.Label(controls, text="", style="Muted.TLabel")
        self.status.pack(side="left", padx=12)
        columns = ("vendor", "product", "location", "class", "configs")
        table = ttk.Treeview(
            self,
            columns=columns,
            show="headings",
            height=6,
            selectmode="browse",
        )
        configure_tree_rows(table)
        for key, label, width in (
            ("vendor", "Vendor", 110),
            ("product", "Product", 110),
            ("location", "Bus / address / port", 230),
            ("class", "Class", 170),
            ("configs", "Configs", 75),
        ):
            table.heading(key, text=label)
            table.column(key, width=width, anchor="w")
        table.pack(fill="x")
        table.bind("<<TreeviewSelect>>", self._selected_descriptor)
        self.table = table
        self.interface_section = ttk.Frame(self)
        ttk.Label(
            self.interface_section,
            text="Supported phone interfaces",
            style="Section.TLabel",
        ).pack(anchor="w", pady=(13, 5))
        self.device_summary = ttk.Label(
            self.interface_section,
            text="Select a phone and inspect it to see interface roles.",
            style="Muted.TLabel",
        )
        self.device_summary.pack(anchor="w", pady=(0, 6))
        interface_columns = (
            "number",
            "role",
            "declared",
            "driver",
            "endpoints",
            "port",
        )
        self.interface_table = ttk.Treeview(
            self.interface_section,
            columns=interface_columns,
            show="headings",
            height=6,
            selectmode="browse",
        )
        configure_tree_rows(self.interface_table)
        for key, label, width in (
            ("number", "No.", 46),
            ("role", "USB role", 200),
            ("declared", "Device name", 175),
            ("driver", "Host driver", 120),
            ("endpoints", "Endpoints", 82),
            ("port", "Serial port", 180),
        ):
            self.interface_table.heading(key, text=label)
            self.interface_table.column(key, width=width, anchor="w")
        self.interface_table.pack(fill="x")
        self.interface_table.bind(
            "<<TreeviewSelect>>", self._selected_interface
        )
        self._details_expanded = False
        self.details_button = ttk.Button(
            self, text="Show technical details", command=self._toggle_details
        )
        self.details_button.pack(anchor="w", pady=(12, 0))
        self.viewer = SyntaxViewer(self, height=14)
        self.on_reveal: Callable[[tk.Misc], None] | None = None
        self.on_layout_change: Callable[[], None] | None = None

    def refresh(self) -> None:
        """Read generic USB descriptors and supported Symbian devices."""
        self.status.configure(text="Refreshing…")
        self._submit(
            lambda: self._client.usb_inventory(),
            self._inventory_ready,
            "Refresh USB inventory",
        )
        if self._refresh_context is not None:
            self._refresh_context()

    def _inventory_ready(self, value: Any, error: Exception | None) -> None:
        if error is not None:
            self.status.configure(text=f"USB: {error}")
            return
        selected = self.table.selection()
        previous = (
            self._descriptors[int(selected[0])]
            if selected and int(selected[0]) < len(self._descriptors)
            else None
        )
        self._descriptors = value
        self.table.delete(*self.table.get_children())
        for index, descriptor in enumerate(self._descriptors):
            location = f"{descriptor.bus} / {descriptor.address}"
            if descriptor.ports:
                location += " / " + ".".join(map(str, descriptor.ports))
            self.table.insert(
                "",
                "end",
                iid=str(index),
                tags=("odd" if index % 2 else "even",),
                values=(
                    f"0x{descriptor.vendor_id:04x}",
                    f"0x{descriptor.product_id:04x}",
                    location,
                    usb_class_name(descriptor.device_class),
                    descriptor.configuration_count,
                ),
            )
        if previous is not None:
            matching = [
                index
                for index, item in enumerate(self._descriptors)
                if (
                    item.vendor_id,
                    item.product_id,
                    item.bus,
                    item.ports,
                )
                == (
                    previous.vendor_id,
                    previous.product_id,
                    previous.bus,
                    previous.ports,
                )
            ]
            if len(matching) == 1:
                self.table.selection_set(str(matching[0]))
            elif self._inspected is None:
                self.viewer.show({})
        self.status.configure(text=f"{len(self._descriptors)} USB devices")

    def set_devices(
        self, devices: tuple[ConnectedDevice, ...], selected: str | None
    ) -> None:
        """Replace live selectors and clear inspection from a removed phone."""
        self._selectors = tuple(device.selector for device in devices)
        self._device_labels = device_labels(devices)
        labels = tuple(self._device_labels)
        self.device_combo.configure(values=labels)
        if (
            self._inspected is not None
            and self._inspected.device.selector not in self._selectors
        ):
            self._clear_inspection()
            self.status.configure(text="Selected phone disconnected")
        self.select_device(selected)

    def select_device(self, selector: str | None) -> None:
        """Follow the phone selected elsewhere in the console."""
        previous = self._device_labels.get(self.selector.get())
        if previous == selector:
            return
        if (
            self._inspected is not None
            and self._inspected.device.selector != selector
        ):
            self._clear_inspection()
        self.selector.set("")
        for label, candidate in self._device_labels.items():
            if candidate == selector:
                self.selector.set(label)
                break
        if selector is not None:
            cached = self._inspection_cache.get(selector)
            if cached is not None:
                self._show_inspection(cached)
            self.inspect()

    def _clear_inspection(self) -> None:
        self._inspected = None
        self.interface_section.pack_forget()
        self.interface_table.delete(*self.interface_table.get_children())
        self.viewer.show({})
        if self.on_layout_change is not None:
            self.on_layout_change()

    def _notify_device(self) -> None:
        selector = self._device_labels.get(self.selector.get())
        if selector and self._on_device_selected is not None:
            self._on_device_selected(selector)

    def _selected_descriptor(self, _event: tk.Event) -> None:
        selected = self.table.selection()
        if selected:
            descriptor = self._descriptors[int(selected[0])]
            self.viewer.show(descriptor.model_dump(mode="json", by_alias=True))

    def inspect(self) -> None:
        """Request the native descriptor map for one selected device."""
        selector = self._device_labels.get(self.selector.get())
        if not selector:
            self.status.configure(text="Select a Symbian device first")
            return
        self.status.configure(text="Inspecting…")
        self._inspection_selector = selector
        self._submit(
            lambda: self._client.inspect(
                DeviceInspectRequest(selector=selector, operation="map")
            ),
            self._inspection_ready,
            "Inspect USB interfaces",
        )

    def _inspection_ready(self, value: Any, error: Exception | None) -> None:
        if error is not None:
            self.status.configure(text=f"Inspection failed: {error}")
            return
        result: DeviceInfoResult = value
        self._inspection_cache[result.device.selector] = result
        if (
            result.device.selector != self._inspection_selector
            or result.device.selector not in self._selectors
            or result.device.selector
            != self._device_labels.get(self.selector.get())
        ):
            self.status.configure(text="Phone changed during inspection")
            return
        self._show_inspection(result)

    def _show_inspection(self, result: DeviceInfoResult) -> None:
        """Apply a ready descriptor map without repeating device I/O."""
        self._inspected = result
        self.status.configure(text=result.device.product)
        self.interface_section.pack(fill="x", before=self.details_button)
        self.device_summary.configure(
            text=(
                f"{result.device.product} · {result.device.interface_profile} "
                f"· {len(result.device.interfaces)} observed interfaces"
            )
        )
        self.interface_table.delete(*self.interface_table.get_children())
        for index, interface in enumerate(result.device.interfaces):
            self.interface_table.insert(
                "",
                "end",
                iid=str(index),
                tags=("odd" if index % 2 else "even",),
                values=(
                    interface.number,
                    interface.function,
                    interface.declared_name or "Unknown",
                    interface.host_driver or "Unknown",
                    (
                        interface.endpoint_count
                        if interface.endpoint_count is not None
                        else "?"
                    ),
                    interface.host_serial_port or "",
                ),
            )
        self.viewer.show(result.model_dump(mode="json", by_alias=True))
        if self.on_layout_change is not None:
            self.on_layout_change()

    def _selected_interface(self, _event: tk.Event) -> None:
        selected = self.interface_table.selection()
        if selected and self._inspected is not None:
            interface = self._inspected.device.interfaces[int(selected[0])]
            self.viewer.show(interface.model_dump(mode="json"))

    def _toggle_details(self) -> None:
        self._details_expanded = not self._details_expanded
        if self._details_expanded:
            self.viewer.pack(fill="both", expand=True, pady=(8, 0))
            self.details_button.configure(text="Hide technical details")
            if self.on_reveal is not None:
                self.on_reveal(self.viewer)
        else:
            self.viewer.pack_forget()
            self.details_button.configure(text="Show technical details")
        if self.on_layout_change is not None:
            self.on_layout_change()

    @property
    def selectors(self) -> tuple[str, ...]:
        """Selectors discovered by the most recent refresh."""
        return self._selectors
