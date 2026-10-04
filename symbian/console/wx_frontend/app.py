"""Native-control desktop shell for the in-process Symbian Console API."""

import sys
from collections.abc import Callable
from datetime import datetime
from typing import Any

import wx

from symbian.console.client import ConsoleClient
from symbian.console.device_status import DeviceStatusTracker
from symbian.console.models import ActivityEntry, CommandCatalog, ConsoleContext
from symbian.console.selection import DeviceSelection
from symbian.console.wx_frontend.actions import ActionPanel
from symbian.console.wx_frontend.bridge import WxRequestBridge
from symbian.console.wx_frontend.devices import ProtocolPanel, UsbPanel
from symbian.console.wx_frontend.navigation import (
    CONTENT,
    SIDEBAR,
    NavigationSidebar,
)
from symbian.console.wx_frontend.widgets import heading, note


class ContextSidebar(wx.Panel):
    """Keep actual working paths and the selected phone in view."""

    def __init__(self, parent: wx.Window, refresh: Callable[[], None]) -> None:
        super().__init__(parent)
        self.SetBackgroundColour(wx.Colour(SIDEBAR))
        self.SetMinSize((235, -1))
        outer = wx.BoxSizer(wx.VERTICAL)
        outer.Add(heading(self, "Current selection", 11), 0, wx.ALL, 12)
        self.fields: dict[str, wx.StaticText] = {}
        for label in (
            "Working directory",
            "Application",
            "SDK",
            "Phone",
            "USB profile",
        ):
            outer.Add(note(self, label), 0, wx.LEFT | wx.RIGHT | wx.TOP, 8)
            value = wx.StaticText(self, label="Checking…")
            value.Wrap(205)
            outer.Add(value, 0, wx.LEFT | wx.RIGHT | wx.TOP, 5)
            self.fields[label] = value
        refresh_button = wx.Button(self, label="Refresh")
        refresh_button.Bind(wx.EVT_BUTTON, lambda event: refresh())
        outer.Add(refresh_button, 0, wx.ALL, 12)
        self.SetSizer(outer)

    def set_context(
        self, context: ConsoleContext, selected: str | None
    ) -> None:
        """Display observed values instead of placeholder labels."""
        device = next(
            (item for item in context.devices if item.selector == selected),
            None,
        )
        values = {
            "Working directory": context.workspace,
            "Application": context.project or "Not selected",
            "SDK": context.sdk_manifest or "Not selected",
            "Phone": (
                f"{device.product}\n{device.vendor_id:04x}:{device.product_id:04x}"
                if device
                else "Not selected"
            ),
            "USB profile": device.interface_profile if device else "Unknown",
        }
        for label, value in values.items():
            self.fields[label].SetLabel(value)
            self.fields[label].Wrap(205)
        self.Layout()


class ActivityPanel(wx.Panel):
    """Show local request history with expandable details."""

    def __init__(self, parent: wx.Window) -> None:
        super().__init__(parent)
        self.SetBackgroundColour(wx.Colour(CONTENT))
        outer = wx.BoxSizer(wx.VERTICAL)
        outer.Add(heading(self, "Session activity"), 0, wx.ALL, 22)
        outer.Add(
            note(self, "Recent SDK and device requests in this window."),
            0,
            wx.LEFT | wx.RIGHT | wx.BOTTOM,
            22,
        )
        self.list = wx.ListCtrl(
            self, style=wx.LC_REPORT | wx.LC_SINGLE_SEL | wx.BORDER_SIMPLE
        )
        for index, (label, width) in enumerate(
            (
                ("Time", 100),
                ("Action", 300),
                ("Outcome", 480),
            )
        ):
            self.list.InsertColumn(index, label, width=width)
        outer.Add(self.list, 1, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 22)
        self.SetSizer(outer)

    def add(self, entry: ActivityEntry) -> None:
        """Prepend a completed operation to the session history."""
        self.list.InsertItem(0, entry.time)
        self.list.SetItem(0, 1, entry.action)
        self.list.SetItem(0, 2, entry.outcome)


class ConsoleFrame(wx.Frame):
    """Compose native panels over one frontend-independent service client."""

    def __init__(self, client: ConsoleClient | None = None) -> None:
        super().__init__(None, title="Symbian Console", size=(1190, 780))
        self.SetMinSize((900, 620))
        self._client = client or ConsoleClient()
        self._bridge = WxRequestBridge()
        self._selection = DeviceSelection()
        self._device_status = DeviceStatusTracker()
        self._context: ConsoleContext | None = None
        self._context_pending = False
        self._activity: list[ActivityEntry] = []
        self._working: list[str] = []
        self.Bind(wx.EVT_CLOSE, self._close)
        self._create_menus()

        shell = wx.Panel(self)
        shell.SetBackgroundColour(wx.Colour(CONTENT))
        shell_sizer = wx.BoxSizer(wx.HORIZONTAL)
        self.pages = wx.Simplebook(shell)
        self.applications = self._actions(
            self.pages,
            "Applications",
            "Create, build, run and package Symbian applications.",
            ("Applications",),
        )
        self.firmware = self._actions(
            self.pages,
            "Firmware library",
            "Import, inspect and export local firmware content.",
            ("Firmware",),
        )
        self.emulator = self._actions(
            self.pages,
            "Emulator",
            "Configure and inspect emulator sessions.",
            ("Emulator",),
        )
        self.sdk_setup = self._actions(
            self.pages,
            "SDK setup",
            "Check this computer and prepare development tools.",
            ("Getting started", "SDK and toolchain"),
        )
        self.sdk_inspection = self._actions(
            self.pages,
            "Artifact inspection",
            "Inspect and verify native outputs.",
            ("Inspection",),
        )
        self.sdk_preservation = self._actions(
            self.pages,
            "Preservation",
            "Create and verify local preservation records.",
            ("Preservation",),
        )
        self.device_actions = self._actions(
            self.pages,
            "Connected phone",
            "Inspect and stage applications on a selected phone.",
            ("Devices",),
        )
        self.usb = UsbPanel(
            self.pages, self._client, self._submit, self._select_device
        )
        self.protocols = ProtocolPanel(
            self.pages, self._client, self._submit, self._select_device
        )
        self.activity = ActivityPanel(self.pages)
        self._page_keys = (
            "applications",
            "firmware",
            "emulator",
            "sdk_setup",
            "sdk_inspection",
            "sdk_preservation",
            "device_actions",
            "usb",
            "protocols",
            "activity",
        )
        for page, label in (
            (self.applications, "Applications"),
            (self.firmware, "Firmware"),
            (self.emulator, "Emulator"),
            (self.sdk_setup, "SDK setup"),
            (self.sdk_inspection, "Inspection"),
            (self.sdk_preservation, "Preservation"),
            (self.device_actions, "Device actions"),
            (self.usb, "USB inspector"),
            (self.protocols, "Protocols"),
            (self.activity, "Activity"),
        ):
            self.pages.AddPage(page, label)
        self.sidebar = ContextSidebar(shell, self.refresh_context)
        self.navigation = NavigationSidebar(
            shell, self._select_page, self.sidebar
        )
        shell_sizer.Add(self.navigation, 0, wx.EXPAND)
        shell_sizer.Add(self.pages, 1, wx.EXPAND)
        shell.SetSizer(shell_sizer)
        status = self.CreateStatusBar(2)
        self.SetStatusWidths([-1, 400])
        self._device_dot = wx.StaticText(status, label="●")
        self._device_text = wx.StaticText(status, label="")
        self._device_dot.Hide()
        self._device_text.Hide()
        status.Bind(wx.EVT_SIZE, self._layout_device_status)
        self.SetStatusText("Connecting to local SDK…", 0)
        self._layout_device_status()
        self._submit(
            self._client.catalog, self._catalog_ready, "Load SDK workflows"
        )
        wx.CallAfter(self.usb.refresh)
        self._timer = wx.Timer(self)
        self.Bind(wx.EVT_TIMER, self._poll_context, self._timer)
        self._timer.Start(5000)

    def _create_menus(self) -> None:
        """Expose standard desktop commands through the system menu bar."""
        bar = wx.MenuBar()
        file_menu = wx.Menu()
        file_menu.Append(wx.ID_EXIT, "Quit\tCtrl+Q")
        bar.Append(file_menu, "File")
        view_menu = wx.Menu()
        view_menu.Append(wx.ID_REFRESH, "Refresh current selections\tCtrl+R")
        self._clear_pending_item = view_menu.Append(
            wx.ID_ANY, "Clear pending device status"
        )
        self._clear_pending_item.Enable(False)
        bar.Append(view_menu, "View")
        help_menu = wx.Menu()
        help_menu.Append(wx.ID_ABOUT, "About Symbian Console")
        bar.Append(help_menu, "Help")
        self.SetMenuBar(bar)
        self.Bind(wx.EVT_MENU, lambda event: self.Close(), id=wx.ID_EXIT)
        self.Bind(
            wx.EVT_MENU,
            lambda event: self.refresh_context(),
            id=wx.ID_REFRESH,
        )
        self.Bind(
            wx.EVT_MENU,
            self._clear_pending_status,
            id=self._clear_pending_item.GetId(),
        )
        self.Bind(
            wx.EVT_MENU,
            lambda event: wx.MessageBox(
                "Symbian Console\nNative desktop frontend for the Symbian SDK.",
                "About Symbian Console",
                wx.OK | wx.ICON_INFORMATION,
                self,
            ),
            id=wx.ID_ABOUT,
        )

    def _actions(
        self,
        parent: wx.Window,
        title: str,
        description: str,
        groups: tuple[str, ...],
    ) -> ActionPanel:
        return ActionPanel(
            parent, self._client, self._submit, title, description, groups
        )

    def _action_panels(self) -> tuple[ActionPanel, ...]:
        return (
            self.applications,
            self.firmware,
            self.emulator,
            self.sdk_setup,
            self.sdk_inspection,
            self.sdk_preservation,
            self.device_actions,
        )

    def _submit(
        self,
        operation: Callable[[], Any],
        completed: Callable[[Any, Exception | None], None],
        label: str,
        device_path: tuple[str, ...] | None = None,
    ) -> None:
        self._working.append(label)
        self.SetStatusText(f"Working · {self._working[0]}", 0)
        token = (
            self._device_status.begin(label, device_path)
            if device_path is not None
            else None
        )
        self._show_device_status()

        def receive(value: Any, error: Exception | None) -> None:
            if not self:
                return
            entry = ActivityEntry(
                time=datetime.now().strftime("%H:%M:%S"),
                action=label,
                outcome=f"Error: {error}" if error else "Completed",
            )
            self._activity.append(entry)
            self.activity.add(entry)
            self._working.remove(label)
            if token is not None:
                self._device_status.finish(token, error is None)
                self._show_device_status()
            self.SetStatusText(
                (
                    f"Working · {self._working[0]}"
                    if self._working
                    else "Ready" if error is None else str(error)
                ),
                0,
            )
            completed(value, error)
            if token is not None:
                self.refresh_context()

        self._bridge.submit(operation, receive)

    def _catalog_ready(self, value: Any, error: Exception | None) -> None:
        if error:
            self.SetStatusText(f"Catalog unavailable: {error}", 0)
            return
        catalog: CommandCatalog = value
        for panel in self._action_panels():
            panel.set_catalog(catalog)
        if not self._working:
            self.SetStatusText(f"Ready · {len(catalog.commands)} workflows", 0)
        self.refresh_context()

    def refresh_context(self) -> None:
        """Observe device and path changes without blocking wx's event loop."""
        if self._context_pending:
            return
        self._context_pending = True
        self._submit(
            self._client.context,
            self._context_ready,
            "Refresh current selections",
        )

    def _context_ready(self, value: Any, error: Exception | None) -> None:
        self._context_pending = False
        if error:
            self._device_status.lose_observation()
            self._show_device_status()
            if not self._working:
                self.SetStatusText(f"Context unavailable: {error}", 0)
            return
        context: ConsoleContext = value
        selector = self._selection.refresh(context.devices)
        self._device_status.observe(context.devices, selector)
        self._show_device_status()
        if context == self._context:
            return
        self._context = context
        self.sidebar.set_context(context, selector)
        for panel in self._action_panels():
            panel.set_context(context)
            panel.select_device(selector)
        self.usb.set_context(context, selector)
        self.protocols.set_context(context, selector)

    def _select_device(self, selector: str) -> None:
        if self._context is None:
            return
        self._selection.select(selector, self._context.devices)
        self._device_status.observe(self._context.devices, selector)
        self._show_device_status()
        self.sidebar.set_context(self._context, selector)
        for panel in self._action_panels():
            panel.select_device(selector)
        self.usb.set_context(self._context, selector)
        self.protocols.set_context(self._context, selector)

    def _show_device_status(self) -> None:
        """Render presence or pending work, leaving idle absence blank."""
        view = self._device_status.view()
        self._clear_pending_item.Enable(self._device_status.has_pending_mode)
        if view is None:
            self._device_dot.Hide()
            self._device_text.Hide()
            return
        colour = "#218c48" if view.tone == "connected" else "#b7791f"
        self._device_dot.SetForegroundColour(wx.Colour(colour))
        self._device_text.SetLabel(view.text)
        self._device_text.SetToolTip(view.text)
        self._device_dot.Show()
        self._device_text.Show()
        self._layout_device_status()

    def _layout_device_status(self, event: wx.SizeEvent | None = None) -> None:
        """Keep the colored bullet and short text inside the native field."""
        rectangle = self.GetStatusBar().GetFieldRect(1)
        self._device_dot.SetPosition((rectangle.x + 8, rectangle.y + 2))
        self._device_text.SetPosition((rectangle.x + 27, rectangle.y + 2))
        self._device_text.SetSize(
            (max(rectangle.width - 33, 0), rectangle.height)
        )
        if event is not None:
            event.Skip()

    def _clear_pending_status(self, _event: wx.CommandEvent) -> None:
        self._device_status.clear_pending()
        self._show_device_status()

    def _select_page(self, key: str) -> None:
        """Switch cached pages immediately and refresh device data later."""
        self.pages.SetSelection(self._page_keys.index(key))
        if key == "usb":
            wx.CallAfter(self.usb.refresh)
        elif key in {"device_actions", "protocols"}:
            wx.CallAfter(self.refresh_context)

    def _poll_context(self, _event: wx.TimerEvent) -> None:
        self.refresh_context()

    def _close(self, event: wx.CloseEvent) -> None:
        self._timer.Stop()
        self._bridge.close()
        event.Skip()


def launch() -> int:
    """Start the native desktop event loop in the detached GUI process."""
    application = wx.App(False)
    application.SetAppName("Symbian Console")
    frame = ConsoleFrame()
    frame.Show()
    application.MainLoop()
    return 0


if __name__ == "__main__":
    sys.exit(launch())
