"""Semi-persistent current workspace, SDK and phone selection."""

import tkinter as tk
from collections.abc import Callable
from tkinter import ttk

from symbian.console.models import ConsoleContext


class ContextSidebar(ttk.Frame):
    """Keep effective selections visible while the main task changes."""

    def __init__(self, parent: tk.Misc, refresh: Callable[[], None]) -> None:
        super().__init__(
            parent, width=235, padding=(13, 15), style="Sidebar.TFrame"
        )
        self.pack_propagate(False)
        self._context: ConsoleContext | None = None
        self._selected_device: str | None = None
        ttk.Label(
            self, text="Current selection", style="SidebarSection.TLabel"
        ).pack(anchor="w", pady=(0, 11))
        self.workspace = self._section("Working directory")
        self.project = self._section("Application")
        self.sdk = self._section("SDK")
        self.phone = self._section("Phone")
        self.connection = self._section("USB profile")
        ttk.Button(self, text="Refresh", command=refresh).pack(
            anchor="w", pady=(9, 0)
        )
        self._render()

    def _section(self, heading: str) -> ttk.Label:
        ttk.Label(self, text=heading, style="SidebarMuted.TLabel").pack(
            anchor="w", pady=(0, 2)
        )
        value = ttk.Label(
            self,
            text="—",
            wraplength=205,
            justify="left",
            style="Sidebar.TLabel",
        )
        value.pack(anchor="w", fill="x", pady=(0, 13))
        return value

    def set_context(self, context: ConsoleContext) -> None:
        """Refresh actual SDK and USB inventory without losing selection."""
        self._context = context
        if self._selected_device not in {
            device.selector for device in context.devices
        }:
            self._selected_device = None
        self._render()

    def select_device(self, selector: str | None) -> None:
        """Track the phone selected in any of the main views."""
        self._selected_device = selector
        self._render()

    def _render(self) -> None:
        if self._context is None:
            return
        context = self._context
        self.workspace.configure(text=context.workspace)
        self.project.configure(
            text=context.project if context.project else "Not selected"
        )
        self.sdk.configure(
            text=(
                context.sdk_manifest if context.sdk_manifest else "Not selected"
            )
        )
        device = next(
            (
                candidate
                for candidate in context.devices
                if candidate.selector == self._selected_device
            ),
            None,
        )
        if device is None:
            self.phone.configure(text="No phone selected")
            self.connection.configure(text="Unknown")
            return
        self.phone.configure(
            text=(
                f"{device.product}\n"
                f"{device.vendor_id:04x}:{device.product_id:04x}"
            )
        )
        self.connection.configure(text=device.interface_profile)
