"""Stock-Tk frontend for the in-process Symbian Console API."""

import sys
import time
import tkinter as tk
from collections.abc import Callable
from datetime import datetime
from tkinter import ttk
from typing import Any

from symbian.console.bridge import TkRequestBridge
from symbian.console.client import ConsoleClient
from symbian.console.devices import UsbInspectorPanel
from symbian.console.models import ActivityEntry, CommandCatalog, ConsoleContext
from symbian.console.protocols import ProtocolPanel
from symbian.console.selection import DeviceSelection
from symbian.console.sidebar import ContextSidebar
from symbian.console.widgets import (
    ScrollableTab,
    SyntaxViewer,
    configure_style,
    configure_tree_rows,
)
from symbian.console.workflows import WorkflowPanel


class ConsoleUnavailable(Exception):
    """The host cannot start the selected GUI frontend."""


class ConsoleWindow:
    """Compose interchangeable frontend panels around one API client."""

    def __init__(
        self, root: tk.Tk, client: ConsoleClient | None = None
    ) -> None:
        self.root = root
        self.client = client or ConsoleClient()
        self.bridge = TkRequestBridge(root)
        self.activity: list[ActivityEntry] = []
        self._context: ConsoleContext | None = None
        self._selection = DeviceSelection()
        self._context_refresh_pending = False
        self._last_usb_refresh = 0.0
        self._last_protocol_refresh = 0.0
        configure_style(root)
        root.title("Symbian Console")
        root.geometry("1190x780")
        root.minsize(890, 620)
        root.protocol("WM_DELETE_WINDOW", self.bridge.close)

        shell = ttk.Frame(root)
        shell.pack(fill="both", expand=True)
        workspace = ttk.Frame(shell)
        workspace.pack(fill="both", expand=True)
        self.sidebar = ContextSidebar(workspace, self._refresh_context)
        self.sidebar.pack(side="right", fill="y")
        ttk.Separator(workspace, orient="vertical").pack(side="right", fill="y")
        self.tabs = ttk.Notebook(workspace)
        self.tabs.pack(side="left", fill="both", expand=True)
        self.applications_tab, self.applications = self._task_panel(
            self.tabs,
            "Applications",
            "Create, build, run and package Symbian applications.",
            ("Applications",),
        )
        self.firmware_tab, self.firmware = self._task_panel(
            self.tabs,
            "Firmware library",
            "Import, inspect and export local firmware content.",
            ("Firmware",),
        )
        self.emulator_tab, self.emulator = self._task_panel(
            self.tabs,
            "Emulator",
            "Configure and inspect emulator sessions.",
            ("Emulator",),
        )
        self.signing_tab, self.signing = self._task_panel(
            self.tabs,
            "Signing",
            "Manage local identities and sign SIS applications.",
            ("Signing",),
        )
        self.sdk_tab = ttk.Notebook(self.tabs)
        self.sdk_setup_tab, self.sdk_setup = self._task_panel(
            self.sdk_tab,
            "SDK setup",
            "Check this computer and prepare development tools.",
            ("Getting started", "SDK and toolchain"),
        )
        self.sdk_inspection_tab, self.sdk_inspection = self._task_panel(
            self.sdk_tab,
            "Artifact inspection",
            "Inspect and verify native outputs and toolchain behavior.",
            ("Inspection",),
        )
        self.sdk_preservation_tab, self.sdk_preservation = self._task_panel(
            self.sdk_tab,
            "Preservation",
            "Create and verify local preservation records.",
            ("Preservation",),
        )
        self.sdk_tab.add(self.sdk_setup_tab, text="Setup")
        self.sdk_tab.add(self.sdk_inspection_tab, text="Inspection")
        self.sdk_tab.add(self.sdk_preservation_tab, text="Preservation")

        self.device_tab = ttk.Notebook(self.tabs)
        self.device_actions_tab, self.device_workflows = self._task_panel(
            self.device_tab,
            "Connected phone",
            "Inspect, configure and stage applications on a selected phone.",
            ("Devices",),
        )
        self.usb_tab = ScrollableTab(self.device_tab)
        self.usb = UsbInspectorPanel(
            self.usb_tab.content,
            self.client,
            self._submit,
            self._select_device,
            self._refresh_context,
        )
        self.usb.pack(fill="both", expand=True)
        self.usb.on_reveal = self.usb_tab.reveal
        self.usb.on_layout_change = self.usb_tab.reflow
        self.protocols_tab = ScrollableTab(self.device_tab)
        self.protocols = ProtocolPanel(
            self.protocols_tab.content,
            self.client,
            self._submit,
            self._select_device,
            self._refresh_context,
        )
        self.protocols.pack(fill="both", expand=True)
        self.protocols.outcome.on_reveal = self.protocols_tab.reveal
        self.protocols.outcome.on_layout_change = self.protocols_tab.reflow
        self.device_tab.add(self.device_actions_tab, text="Actions")
        self.device_tab.add(self.usb_tab, text="USB inspector")
        self.device_tab.add(self.protocols_tab, text="Protocols")
        self.device_tab.bind("<<NotebookTabChanged>>", self._tab_changed)
        self.activity_tab = ScrollableTab(self.tabs)
        self.activity_content = ttk.Frame(self.activity_tab.content, padding=18)
        self.activity_content.pack(fill="both", expand=True)
        self.tabs.add(self.applications_tab, text="Applications")
        self.tabs.add(self.firmware_tab, text="Firmware")
        self.tabs.add(self.emulator_tab, text="Emulator")
        self.tabs.add(self.signing_tab, text="Signing")
        self.tabs.add(self.sdk_tab, text="SDK tools")
        self.tabs.add(self.device_tab, text="Devices")
        self.tabs.add(self.activity_tab, text="Activity")
        self.tabs.bind("<<NotebookTabChanged>>", self._tab_changed)
        self._make_activity()

        ttk.Separator(shell).pack(fill="x")
        footer = ttk.Frame(shell, padding=(12, 5))
        footer.pack(fill="x")
        self.status = ttk.Label(
            footer, text="Connecting to local SDK…", style="Muted.TLabel"
        )
        self.status.pack(side="left")
        self.context_status = ttk.Label(
            footer, text="Checking current selections…", style="Muted.TLabel"
        )
        self.context_status.pack(side="right")
        self._submit(
            lambda: self.client.catalog(),
            self._catalog_ready,
            "Load SDK workflows",
        )
        root.after(100, self.usb.refresh)
        root.after(5000, self._poll_context)

    def _task_panel(
        self,
        parent: tk.Misc,
        heading: str,
        summary: str,
        groups: tuple[str, ...],
    ) -> tuple[ScrollableTab, WorkflowPanel]:
        """Create a section-local guided action view."""
        viewport = ScrollableTab(parent)
        panel = WorkflowPanel(
            viewport.content,
            self.client,
            self._submit,
            self._select_device,
            heading=heading,
            summary=summary,
            groups=groups,
        )
        panel.pack(fill="both", expand=True)
        panel.outcome.on_reveal = viewport.reveal
        panel.outcome.on_layout_change = viewport.reflow
        panel.on_outer_wheel = viewport.scroll_wheel
        panel.on_layout_change = viewport.reflow
        return viewport, panel

    def _poll_context(self) -> None:
        """Observe plug and unplug transitions without blocking Tk."""
        self._refresh_context()
        self.root.after(5000, self._poll_context)

    def _tab_changed(self, _event: tk.Event) -> None:
        """Show cached widgets now, then refresh the active device view."""
        if self.tabs.select() != str(self.device_tab):
            return
        current = self.device_tab.select()
        now = time.monotonic()
        if current == str(self.usb_tab) and now - self._last_usb_refresh > 10:
            self._last_usb_refresh = now
            self.root.after_idle(self.usb.refresh)
            if self._selection.current is not None:
                self.root.after_idle(self.usb.inspect)
        elif (
            current == str(self.protocols_tab)
            and now - self._last_protocol_refresh > 10
        ):
            self._last_protocol_refresh = now
            self.root.after_idle(self._refresh_context)

    def _refresh_context(self) -> None:
        """Refresh the current paths and phone inventory for every view."""
        if self._context_refresh_pending:
            return
        self._context_refresh_pending = True
        self._submit(
            lambda: self.client.context(),
            self._context_ready,
            "Refresh current selections",
        )

    def _context_ready(self, value: Any, error: Exception | None) -> None:
        self._context_refresh_pending = False
        if error is not None:
            self.context_status.configure(text=f"Context unavailable: {error}")
            return
        context: ConsoleContext = value
        self._context = context
        selected = self._selection.refresh(context.devices)
        self.sidebar.set_context(context)
        for panel in self._workflow_panels():
            panel.set_context(context)
        self.usb.set_devices(context.devices, selected)
        self.protocols.set_devices(context.devices, selected)
        if selected:
            self._select_device(selected)
        else:
            self._clear_device()
        self.context_status.configure(
            text=(
                f"{len(context.devices)} phone(s) · "
                f"SDK {'selected' if context.sdk_manifest else 'not selected'}"
            )
        )

    def _select_device(self, selector: str) -> None:
        """Apply one selected phone across the persistent sidebar and views."""
        if self._context is None:
            return
        self._selection.select(selector, self._context.devices)
        if self._selection.current is None:
            return
        self.sidebar.select_device(selector)
        self.usb.select_device(selector)
        self.protocols.select_device(selector)
        for panel in self._workflow_panels():
            panel.select_device(selector)

    def _clear_device(self) -> None:
        """Remove stale phone selectors from every view."""
        self.sidebar.select_device(None)
        self.usb.select_device(None)
        self.protocols.select_device(None)
        for panel in self._workflow_panels():
            panel.select_device(None)

    def _workflow_panels(self) -> tuple[WorkflowPanel, ...]:
        """Return the persistent guided sections across all tabs."""
        return (
            self.applications,
            self.firmware,
            self.emulator,
            self.signing,
            self.sdk_setup,
            self.sdk_inspection,
            self.sdk_preservation,
            self.device_workflows,
        )

    def _make_activity(self) -> None:
        ttk.Label(
            self.activity_content,
            text="Session activity",
            style="Title.TLabel",
        ).pack(anchor="w", pady=(0, 5))
        ttk.Label(
            self.activity_content,
            text="Recent local requests and their outcomes.",
            style="Muted.TLabel",
        ).pack(anchor="w", pady=(0, 14))
        self.activity_table = ttk.Treeview(
            self.activity_content,
            columns=("time", "action", "outcome"),
            show="headings",
            height=15,
        )
        configure_tree_rows(self.activity_table)
        for name, width in (("time", 110), ("action", 360), ("outcome", 390)):
            self.activity_table.heading(name, text=name.title())
            self.activity_table.column(name, width=width)
        self.activity_table.pack(fill="both", expand=True)
        self.activity_viewer = SyntaxViewer(self.activity_content, height=14)
        self.activity_table.bind("<<TreeviewSelect>>", self._selected_activity)

    def _submit(
        self,
        operation: Callable[[], Any],
        completed: Callable[[Any, Exception | None], None],
        label: str,
    ) -> None:
        self.status.configure(text=f"Working · {label}")

        def receive(value: Any, error: Exception | None) -> None:
            outcome = f"Error: {error}" if error else "Completed"
            entry = ActivityEntry(
                time=datetime.now().strftime("%H:%M:%S"),
                action=label,
                outcome=outcome,
            )
            self.activity.append(entry)
            self.activity_table.insert(
                "",
                0,
                iid=str(len(self.activity) - 1),
                tags=("odd" if len(self.activity) % 2 else "even",),
                values=(entry.time, entry.action, entry.outcome),
            )
            self.status.configure(
                text="Ready" if not self.bridge.busy else "Working…"
            )
            completed(value, error)

        self.bridge.submit(operation, receive)

    def _catalog_ready(self, value: Any, error: Exception | None) -> None:
        if error is not None:
            self.status.configure(text=f"Catalog unavailable: {error}")
            return
        catalog: CommandCatalog = value
        for panel in self._workflow_panels():
            panel.set_catalog(catalog)
        self.status.configure(text=f"Ready · {len(catalog.commands)} workflows")

    def _selected_activity(self, _event: tk.Event) -> None:
        selected = self.activity_table.selection()
        if selected:
            entry = self.activity[int(selected[0])]
            self.activity_viewer.show(entry.model_dump(mode="json"))
            self.activity_viewer.pack(fill="both", pady=(12, 0))
            self.activity_tab.reflow()
            self.activity_tab.reveal(self.activity_viewer)


def launch() -> int:
    """Open the desktop frontend, reporting missing display support cleanly."""
    try:
        root = tk.Tk()
    except tk.TclError as error:
        raise ConsoleUnavailable(str(error)) from error
    try:
        ConsoleWindow(root)
        root.mainloop()
    except KeyboardInterrupt:
        root.destroy()
    return 0


if __name__ == "__main__":
    sys.exit(launch())
