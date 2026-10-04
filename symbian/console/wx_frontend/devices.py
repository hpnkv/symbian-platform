"""Native USB inventory and phone protocol views."""

from collections.abc import Callable
from typing import Any

import wx

from symbian.console.client import ConsoleClient
from symbian.console.models import (
    ConsoleContext,
    DeviceInspectRequest,
    OutcomeFact,
    OutcomeSummary,
)
from symbian.console.selection import device_labels
from symbian.console.wx_frontend.navigation import CONTENT
from symbian.console.wx_frontend.widgets import (
    ResultPanel,
    RoundedCard,
    heading,
    note,
)
from symbian.device.connection import ConnectedDevice, DeviceInfoResult
from symbian.device.usb_models import UsbDeviceDescriptor
from symbian.status import Status

Submit = Callable[
    [
        Callable[[], Any],
        Callable[[Any, Exception | None], None],
        str,
        tuple[str, ...] | None,
    ],
    None,
]


class PhonePicker(wx.Panel):
    """Present live phone names and preserve a serial-backed selection."""

    def __init__(
        self, parent: wx.Window, selected: Callable[[str], None]
    ) -> None:
        super().__init__(parent)
        self.SetBackgroundColour(wx.Colour(CONTENT))
        self._selected = selected
        self._labels: dict[str, str] = {}
        row = wx.BoxSizer(wx.HORIZONTAL)
        row.Add(
            wx.StaticText(self, label="Phone"),
            0,
            wx.ALIGN_CENTER_VERTICAL | wx.RIGHT,
            8,
        )
        self.choice = wx.Choice(self)
        self.choice.Bind(wx.EVT_CHOICE, self._changed)
        row.Add(self.choice, 1)
        self.SetSizer(row)

    def set_devices(
        self, devices: tuple[ConnectedDevice, ...], selector: str | None
    ) -> None:
        """Refresh live choices while retaining the selected identity."""
        self._labels = device_labels(devices)
        self.choice.Set(list(self._labels))
        for index, item in enumerate(self._labels.values()):
            if item == selector:
                self.choice.SetSelection(index)
                break

    def _changed(self, _event: wx.CommandEvent) -> None:
        selector = self._labels.get(self.choice.GetStringSelection())
        if selector:
            self._selected(selector)


def _result_summary(operation: str, result: DeviceInfoResult) -> OutcomeSummary:
    """Extract the most useful observed facts for a protocol result."""
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
    return OutcomeSummary(
        title=(
            f"{result.device.product} · "
            f"{operation.replace('-', ' ').title()}"
        ),
        message=result.scope,
        facts=tuple(facts),
    )


class UsbPanel(wx.ScrolledWindow):
    """Browse generic host USB devices and inspect a selected phone."""

    def __init__(
        self,
        parent: wx.Window,
        client: ConsoleClient,
        submit: Submit,
        selected: Callable[[str], None],
    ) -> None:
        super().__init__(parent, style=wx.VSCROLL | wx.TAB_TRAVERSAL)
        self.SetBackgroundColour(wx.Colour(CONTENT))
        self.SetScrollRate(0, 12)
        self._client, self._submit, self._selected = client, submit, selected
        self._inventory: tuple[UsbDeviceDescriptor, ...] = ()
        self._selector: str | None = None
        outer = wx.BoxSizer(wx.VERTICAL)
        outer.Add(heading(self, "USB inspector"), 0, wx.ALL, 22)
        outer.Add(
            note(
                self,
                "Explore host descriptors and the selected phone's "
                "interface roles.",
            ),
            0,
            wx.LEFT | wx.RIGHT | wx.BOTTOM,
            16,
        )
        toolbar = wx.BoxSizer(wx.HORIZONTAL)
        refresh = wx.Button(self, label="Refresh inventory")
        refresh.Bind(wx.EVT_BUTTON, lambda event: self.refresh())
        toolbar.Add(refresh, 0, wx.RIGHT, 8)
        self.picker = PhonePicker(self, selected)
        toolbar.Add(self.picker, 1, wx.RIGHT, 8)
        inspect = wx.Button(self, label="Inspect phone")
        inspect.Bind(wx.EVT_BUTTON, lambda event: self.inspect())
        toolbar.Add(inspect)
        outer.Add(toolbar, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 16)
        self.table = wx.ListCtrl(
            self,
            style=wx.LC_REPORT | wx.LC_SINGLE_SEL | wx.BORDER_SIMPLE,
            size=(-1, 240),
        )
        for index, (label, width) in enumerate(
            (
                ("Vendor", 110),
                ("Product", 110),
                ("Location", 200),
                ("Class", 100),
                ("Configs", 95),
            )
        ):
            self.table.InsertColumn(index, label, width=width)
        self.table.Bind(wx.EVT_LIST_ITEM_SELECTED, self._show_descriptor)
        outer.Add(self.table, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 16)
        self.result = ResultPanel(self)
        outer.Add(
            self.result, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 16
        )
        self.SetSizer(outer)

    def set_context(
        self, context: ConsoleContext, selector: str | None
    ) -> None:
        """Update phone choices and clear stale phone results on unplug."""
        changed = selector != self._selector
        self._selector = selector
        self.picker.set_devices(context.devices, selector)
        if changed:
            self.result.Hide()
            self.Layout()

    def refresh(self) -> None:
        """Refresh host inventory in the worker while keeping cached rows."""
        self._submit(
            self._client.usb_inventory,
            self._inventory_ready,
            "Refresh USB inventory",
            None,
        )

    def _inventory_ready(self, value: Any, error: Exception | None) -> None:
        if error:
            self._show_error(error)
            return
        selected_row = self.table.GetFirstSelected()
        previous = (
            self._inventory[selected_row]
            if 0 <= selected_row < len(self._inventory)
            else None
        )
        self._inventory = value
        self.table.DeleteAllItems()
        alternate = wx.Colour("#f2f3f5")
        for index, device in enumerate(self._inventory):
            row = self.table.InsertItem(index, f"0x{device.vendor_id:04x}")
            self.table.SetItem(row, 1, f"0x{device.product_id:04x}")
            self.table.SetItem(
                row,
                2,
                (
                    f"{device.bus} / {device.address} / "
                    + ".".join(str(port) for port in device.ports)
                ),
            )
            self.table.SetItem(row, 3, f"0x{device.device_class:02x}")
            self.table.SetItem(row, 4, str(device.configuration_count))
            if index % 2:
                self.table.SetItemBackgroundColour(row, alternate)
        if previous is not None:
            matches = [
                index
                for index, device in enumerate(self._inventory)
                if (
                    device.vendor_id,
                    device.product_id,
                    device.bus,
                    device.ports,
                )
                == (
                    previous.vendor_id,
                    previous.product_id,
                    previous.bus,
                    previous.ports,
                )
            ]
            if len(matches) == 1:
                self.table.Select(matches[0])

    def _show_descriptor(self, event: wx.ListEvent) -> None:
        descriptor = self._inventory[event.GetIndex()]
        self.result.show(
            OutcomeSummary(
                title=(
                    f"USB {descriptor.vendor_id:04x}:"
                    f"{descriptor.product_id:04x}"
                ),
                message="Host-reported descriptor identity and location.",
                facts=(
                    OutcomeFact(
                        label="Bus / address",
                        value=(f"{descriptor.bus} / {descriptor.address}"),
                    ),
                ),
            ),
            descriptor.model_dump(mode="json"),
        )
        self.Layout()

    def inspect(self) -> None:
        """Request the native USB map for the selected phone."""
        if not self._selector:
            self.result.show(
                OutcomeSummary(
                    title="Select a phone",
                    message="Choose a connected phone above.",
                ),
                {},
            )
            return
        request = DeviceInspectRequest(selector=self._selector, operation="map")
        self._submit(
            lambda: self._client.inspect(request),
            self._inspected,
            "Inspect USB map",
            ("device", "usb", "map"),
        )

    def _inspected(self, value: Any, error: Exception | None) -> None:
        if error:
            self._show_error(error)
            return
        self.result.show(
            _result_summary("map", value), value.model_dump(mode="json")
        )
        self.Layout()

    def _show_error(self, error: Exception) -> None:
        status = Status.from_exception(error)
        self.result.show(
            OutcomeSummary(title="Inspection failed", message=str(status)),
            status.model_dump(mode="json"),
        )


class ProtocolPanel(wx.ScrolledWindow):
    """Offer bounded AT, MTP and PC Suite OBEX operations by purpose."""

    def __init__(
        self,
        parent: wx.Window,
        client: ConsoleClient,
        submit: Submit,
        selected: Callable[[str], None],
    ) -> None:
        super().__init__(parent, style=wx.VSCROLL | wx.TAB_TRAVERSAL)
        self.SetBackgroundColour(wx.Colour(CONTENT))
        self.SetScrollRate(0, 12)
        self._client, self._submit = client, submit
        self._selector: str | None = None
        outer = wx.BoxSizer(wx.VERTICAL)
        outer.Add(heading(self, "Device protocols"), 0, wx.ALL, 22)
        outer.Add(
            note(
                self,
                "Read information through the phone's available interfaces.",
            ),
            0,
            wx.LEFT | wx.RIGHT | wx.BOTTOM,
            16,
        )
        self.picker = PhonePicker(self, selected)
        outer.Add(
            self.picker, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 16
        )
        self._section(
            outer,
            "AT modem",
            "Phone identity, battery and signal codes.",
            (
                ("Read identity", "at-identity"),
                ("Read status", "at-status"),
            ),
        )
        mtp_content, mtp_sizer = self._section(
            outer,
            "MTP / PTP",
            "Device metadata and a bounded root listing.",
            (
                ("Read metadata", "mtp"),
                ("List root objects", "mtp-list"),
            ),
        )
        limit_row = wx.BoxSizer(wx.HORIZONTAL)
        limit_row.Add(
            wx.StaticText(mtp_content, label="Items per storage"),
            0,
            wx.ALIGN_CENTER_VERTICAL | wx.RIGHT,
            10,
        )
        self.limit = wx.SpinCtrl(mtp_content, min=0, max=128, initial=8)
        limit_row.Add(self.limit)
        mtp_sizer.Add(limit_row, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 12)
        mtp_content.Layout()
        self._section(
            outer,
            "PC Suite OBEX",
            "Connect and disconnect without browsing or transfer.",
            (("Connect and disconnect", "obex"),),
        )
        self.result_card = RoundedCard(self)
        result_content = self.result_card.content
        result_sizer = wx.BoxSizer(wx.VERTICAL)
        self.result = ResultPanel(result_content)
        result_sizer.Add(self.result, 0, wx.EXPAND | wx.ALL, 12)
        result_content.SetSizer(result_sizer)
        outer.Add(
            self.result_card, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 16
        )
        self.result_card.Hide()
        self.SetSizer(outer)

    def _section(
        self,
        outer: wx.BoxSizer,
        title: str,
        description: str,
        operations: tuple[tuple[str, str], ...],
    ) -> tuple[wx.Panel, wx.BoxSizer]:
        card = RoundedCard(self)
        content = card.content
        box = wx.BoxSizer(wx.VERTICAL)
        box.Add(heading(content, title, 13), 0, wx.ALL, 12)
        box.Add(
            note(content, description), 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 12
        )
        row = wx.BoxSizer(wx.HORIZONTAL)
        for label, operation in operations:
            button = wx.Button(content, label=label)
            button.Bind(
                wx.EVT_BUTTON, lambda event, name=operation: self.inspect(name)
            )
            row.Add(button, 0, wx.RIGHT, 8)
        box.Add(row, 0, wx.LEFT | wx.RIGHT | wx.BOTTOM, 12)
        content.SetSizer(box)
        outer.Add(card, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 16)
        return content, box

    def set_context(
        self, context: ConsoleContext, selector: str | None
    ) -> None:
        changed = selector != self._selector
        self._selector = selector
        self.picker.set_devices(context.devices, selector)
        if changed:
            self.result_card.Hide()
            self.FitInside()

    def inspect(self, operation: str) -> None:
        if not self._selector:
            self._show_result(
                OutcomeSummary(
                    title="Select a phone",
                    message="Choose a connected phone above.",
                ),
                {},
            )
            return
        request = DeviceInspectRequest(
            selector=self._selector,
            operation=operation,
            limit=self.limit.GetValue(),
        )
        self._submit(
            lambda: self._client.inspect(request),
            lambda value, error: self._inspected(operation, value, error),
            f"Read {operation}",
            ("device", "protocol", operation),
        )

    def _inspected(
        self, operation: str, value: Any, error: Exception | None
    ) -> None:
        if error:
            status = Status.from_exception(error)
            self._show_result(
                OutcomeSummary(
                    title="Protocol request failed", message=str(status)
                ),
                status.model_dump(mode="json"),
            )
        else:
            self._show_result(
                _result_summary(operation, value), value.model_dump(mode="json")
            )

    def _show_result(self, summary: OutcomeSummary, payload: Any) -> None:
        self.result_card.Show()
        self.result.show(summary, payload)
        self.Layout()
        self.FitInside()
