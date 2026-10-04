"""Reusable Tk widgets with a native light appearance."""

import json
import sys
import tkinter as tk
import tkinter.font as tkfont
from collections.abc import Callable
from tkinter import ttk
from typing import Any

from symbian.console.models import OutcomeSummary
from symbian.console.syntax import LANGUAGES, highlighted_spans

BACKGROUND = "#ffffff"
SURFACE = "#ffffff"
SURFACE_RAISED = "#f3f4f6"
SIDEBAR = "#f6f6f8"
TREE_ALTERNATE = "#f4f5f5"
TEXT = "#1f2328"
MUTED = "#526173"
ACCENT = "#0064e1"
ERROR = "#b42336"


def _system_colour(root: tk.Tk, name: str, fallback: str) -> str:
    """Read a native colour when the current Tk platform exposes it."""
    try:
        return "#{:02x}{:02x}{:02x}".format(
            *(part >> 8 for part in root.winfo_rgb(name))
        )
    except tk.TclError:
        return fallback


def configure_tree_rows(tree: ttk.Treeview) -> None:
    """Use alternating surfaces on native tree controls."""
    alternate = (
        _system_colour(
            tree.winfo_toplevel(),
            "systemAlternatingContentBackgroundColor",
            TREE_ALTERNATE,
        )
        if sys.platform == "darwin"
        else TREE_ALTERNATE
    )
    tree.tag_configure("even", background=SURFACE)
    tree.tag_configure("odd", background=alternate)


class ScrollableTab(ttk.Frame):
    """Keep a long view scrollable while its sidebar and status stay fixed."""

    def __init__(self, parent: tk.Misc) -> None:
        super().__init__(parent)
        self.canvas = tk.Canvas(
            self,
            background=BACKGROUND,
            borderwidth=0,
            highlightthickness=0,
        )
        self.scrollbar = ttk.Scrollbar(
            self, orient="vertical", command=self.canvas.yview
        )
        self.canvas.configure(yscrollcommand=self.scrollbar.set)
        self.canvas.pack(side="left", fill="both", expand=True)
        self.content = ttk.Frame(self.canvas)
        self._window = self.canvas.create_window(
            (0, 0), window=self.content, anchor="nw"
        )
        self.canvas.bind("<Configure>", self._resize)
        self.content.bind("<Configure>", self._resize)
        self.bind_all("<MouseWheel>", self._mousewheel, add="+")
        self.bind_all("<Button-4>", self._mousewheel, add="+")
        self.bind_all("<Button-5>", self._mousewheel, add="+")

    def _resize(self, _event: tk.Event) -> None:
        self.after_idle(self._sync)

    def reflow(self) -> None:
        """Recalculate the scroll extent after child content changes size."""
        self.after_idle(self._sync)

    def _sync(self) -> None:
        if not self.winfo_exists():
            return
        height = max(self.canvas.winfo_height(), self.content.winfo_reqheight())
        self.canvas.itemconfigure(
            self._window, width=self.canvas.winfo_width(), height=height
        )
        self.canvas.configure(
            scrollregion=(0, 0, self.canvas.winfo_width(), height)
        )
        if height > self.canvas.winfo_height() + 2:
            if not self.scrollbar.winfo_manager():
                self.scrollbar.pack(side="right", fill="y")
        elif self.scrollbar.winfo_manager():
            self.scrollbar.pack_forget()

    def _mousewheel(self, event: tk.Event) -> None:
        widget = event.widget
        if not str(widget).startswith(str(self.content)):
            return
        direction = (
            -1
            if getattr(event, "num", 0) == 4 or getattr(event, "delta", 0) > 0
            else 1
        )
        if isinstance(widget, (tk.Text, ttk.Treeview)):
            top, bottom = widget.yview()
            if (direction < 0 and top > 0) or (direction > 0 and bottom < 1):
                return
        if self.content.winfo_reqheight() <= self.canvas.winfo_height():
            return
        self.scroll_wheel(direction)

    def scroll_wheel(self, direction: int) -> None:
        """Scroll this viewport when a nested control reaches its edge."""
        if self.content.winfo_reqheight() > self.canvas.winfo_height():
            self.canvas.yview_scroll(direction * 3, "units")

    def reveal(self, widget: tk.Misc) -> None:
        """Scroll enough to expose a detail area after it is opened."""
        self.after_idle(lambda: self._reveal(widget))

    def _reveal(self, widget: tk.Misc) -> None:
        if not self.winfo_exists() or not widget.winfo_exists():
            return
        self.update_idletasks()
        self._sync()
        bottom = (
            widget.winfo_rooty()
            - self.content.winfo_rooty()
            + widget.winfo_height()
            + 12
        )
        visible_bottom = self.canvas.canvasy(self.canvas.winfo_height())
        if bottom > visible_bottom:
            total_height = max(self.content.winfo_height(), 1)
            self.canvas.yview_moveto(
                max(0, bottom - self.canvas.winfo_height()) / total_height
            )


def configure_style(root: tk.Tk) -> None:
    """Use native desktop controls with a restrained light palette."""
    root.configure(background=BACKGROUND)
    style = ttk.Style(root)
    native_theme = (
        "aqua"
        if sys.platform == "darwin"
        else "vista" if sys.platform == "win32" else "clam"
    )
    if native_theme in style.theme_names():
        style.theme_use(native_theme)
    default_font = tkfont.nametofont("TkDefaultFont")
    font_family = default_font.actual("family")
    font_size = abs(int(default_font.actual("size")))
    style.configure("TFrame", background=BACKGROUND)
    style.configure("Card.TFrame", background=SURFACE)
    style.configure("Sidebar.TFrame", background=SIDEBAR)
    style.configure("TLabel", background=BACKGROUND, foreground=TEXT)
    style.configure("Muted.TLabel", background=BACKGROUND, foreground=MUTED)
    style.configure("Sidebar.TLabel", background=SIDEBAR, foreground=TEXT)
    style.configure("SidebarMuted.TLabel", background=SIDEBAR, foreground=MUTED)
    style.configure(
        "SidebarSection.TLabel",
        background=SIDEBAR,
        foreground=TEXT,
        font=(font_family, font_size + 1, "bold"),
    )
    style.configure("Card.TLabel", background=SURFACE, foreground=TEXT)
    style.configure("CardMuted.TLabel", background=SURFACE, foreground=MUTED)
    style.configure("Title.TLabel", font=(font_family, font_size + 5, "bold"))
    style.configure("Section.TLabel", font=(font_family, font_size + 1, "bold"))
    style.configure("Accent.TButton", font=(font_family, font_size, "bold"))
    if native_theme == "clam":
        style.configure("TButton", background=SURFACE_RAISED, padding=(10, 6))
        style.configure("Accent.TButton", background=ACCENT, foreground=SURFACE)
        style.map("Accent.TButton", background=[("active", "#09549e")])
        style.configure("TEntry", fieldbackground=SURFACE, foreground=TEXT)
        style.configure("TCombobox", fieldbackground=SURFACE, foreground=TEXT)
    style.configure("TNotebook", background=BACKGROUND)
    style.configure(
        "Treeview",
        background=SURFACE,
        fieldbackground=SURFACE,
        foreground=TEXT,
        rowheight=29,
    )
    if native_theme == "clam":
        style.configure("Treeview.Heading", background=SURFACE_RAISED)
    selection = (
        _system_colour(root, "systemSelectedContentBackgroundColor", ACCENT)
        if sys.platform == "darwin"
        else _system_colour(root, "SystemHighlight", ACCENT)
    )
    selection_text = (
        SURFACE
        if sys.platform == "darwin"
        else _system_colour(root, "SystemHighlightText", SURFACE)
    )
    style.map(
        "Treeview",
        background=[("selected", selection)],
        foreground=[("selected", selection_text)],
    )


def title(parent: tk.Misc, heading: str, subtitle: str) -> ttk.Frame:
    """Create a title and short task description."""
    frame = ttk.Frame(parent)
    ttk.Label(frame, text=heading, style="Title.TLabel").pack(anchor="w")
    ttk.Label(frame, text=subtitle, style="Muted.TLabel").pack(
        anchor="w", pady=(4, 0)
    )
    return frame


class SyntaxViewer(ttk.Frame):
    """Read-only technical output with explicit language highlighting."""

    def __init__(self, parent: tk.Misc, height: int = 18) -> None:
        super().__init__(parent)
        toolbar = ttk.Frame(self)
        toolbar.pack(fill="x", pady=(0, 7))
        ttk.Label(toolbar, text="Details", style="Section.TLabel").pack(
            side="left"
        )
        ttk.Button(toolbar, text="Copy", command=self._copy).pack(side="right")
        self.language = tk.StringVar(value="JSON")
        selector = ttk.Combobox(
            toolbar,
            textvariable=self.language,
            values=tuple(LANGUAGES),
            state="readonly",
            width=13,
        )
        selector.pack(side="right", padx=(0, 8))
        selector.bind("<<ComboboxSelected>>", lambda _event: self._render())
        body = ttk.Frame(self)
        body.pack(fill="both", expand=True)
        code_font = tkfont.nametofont("TkFixedFont").copy()
        code_font.configure(
            size=tkfont.nametofont("TkDefaultFont").actual("size")
        )
        self._code_font = code_font
        self.text = tk.Text(
            body,
            wrap="none",
            height=height,
            background=SURFACE,
            foreground=TEXT,
            insertbackground=TEXT,
            selectbackground="#c9ddf7",
            relief="flat",
            borderwidth=0,
            highlightthickness=0,
            padx=10,
            pady=10,
            font=code_font,
        )
        vertical = ttk.Scrollbar(
            body, orient="vertical", command=self.text.yview
        )
        horizontal = ttk.Scrollbar(
            body, orient="horizontal", command=self.text.xview
        )
        self.text.configure(
            yscrollcommand=vertical.set, xscrollcommand=horizontal.set
        )
        self.text.grid(row=0, column=0, sticky="nsew")
        vertical.grid(row=0, column=1, sticky="ns")
        horizontal.grid(row=1, column=0, sticky="ew")
        body.grid_rowconfigure(0, weight=1)
        body.grid_columnconfigure(0, weight=1)
        for category, colour in {
            "comment": MUTED,
            "error": ERROR,
            "string": "#167043",
            "number": "#9b5400",
            "keyword": "#713cad",
            "tag": "#075fa5",
            "attribute": "#9b5400",
            "operator": "#56616f",
        }.items():
            self.text.tag_configure(category, foreground=colour)
        self._source = ""

    def show(self, value: Any, language: str = "JSON") -> None:
        """Display typed SDK data or already formatted technical text."""
        if isinstance(value, str):
            self._source = value
        else:
            self._source = json.dumps(
                value, indent=2, ensure_ascii=False, default=str
            )
        self.language.set(language)
        self._render()

    def _render(self) -> None:
        self.text.configure(state="normal")
        self.text.delete("1.0", "end")
        source = self._source
        if len(source) > 524288:
            source = source[:524288] + "\n\n… display truncated at 512 KiB"
        for category, fragment in highlighted_spans(
            source, self.language.get()
        ):
            self.text.insert("end", fragment, category)
        self.text.configure(state="disabled")

    def _copy(self) -> None:
        self.clipboard_clear()
        self.clipboard_append(self._source)


class OutcomePanel(ttk.Frame):
    """Show a useful result summary with optional technical detail."""

    def __init__(
        self,
        parent: tk.Misc,
        on_reveal: Callable[[tk.Misc], None] | None = None,
    ) -> None:
        super().__init__(parent)
        self.on_reveal = on_reveal
        self.on_layout_change: Callable[[], None] | None = None
        self.bind("<Configure>", self._resize)
        ttk.Separator(self).pack(fill="x", pady=(0, 10))
        self.heading = ttk.Label(self, text="", style="Section.TLabel")
        self.heading.pack(anchor="w")
        self.message = ttk.Label(
            self,
            text="",
            style="Muted.TLabel",
            wraplength=660,
        )
        self.message.pack(anchor="w", pady=(4, 7))
        self.facts = ttk.Frame(self)
        self.facts.pack(fill="x")
        self._value_labels: list[ttk.Label] = []
        self.expanded = False
        self.toggle = ttk.Button(
            self, text="Show technical details", command=self._toggle
        )
        self.toggle.pack(anchor="w", pady=(8, 0))
        self.viewer = SyntaxViewer(self, height=14)

    def _resize(self, event: tk.Event) -> None:
        self.message.configure(wraplength=max(280, event.width - 20))
        for label in self._value_labels:
            label.configure(wraplength=max(120, event.width - 210))

    def show(self, summary: OutcomeSummary, details: Any) -> None:
        """Replace the visible summary and keep full data one click away."""
        self.heading.configure(text=summary.title)
        self.message.configure(text=summary.message)
        for child in self.facts.winfo_children():
            child.destroy()
        self._value_labels = []
        for fact in summary.facts:
            row = ttk.Frame(self.facts)
            row.pack(fill="x", pady=(0, 3))
            ttk.Label(
                row,
                text=fact.label + ":",
                style="Muted.TLabel",
                width=20,
            ).pack(side="left")
            value = ttk.Label(
                row,
                text=fact.value,
                wraplength=max(120, self.winfo_width() - 210),
            )
            value.pack(side="left")
            self._value_labels.append(value)
        self.viewer.show(details)
        if self.expanded:
            self.viewer.pack_forget()
            self.expanded = False
        self.toggle.configure(text="Show technical details")
        self.pack(fill="x", pady=(9, 0))
        if self.on_layout_change is not None:
            self.on_layout_change()

    def hide(self) -> None:
        """Remove an earlier result when the user switches tasks."""
        self.pack_forget()
        if self.on_layout_change is not None:
            self.on_layout_change()

    def _toggle(self) -> None:
        self.expanded = not self.expanded
        if self.expanded:
            self.viewer.pack(fill="both", expand=True, pady=(9, 0))
            self.toggle.configure(text="Hide technical details")
            if self.on_reveal is not None:
                self.on_reveal(self.viewer)
        else:
            self.viewer.pack_forget()
            self.toggle.configure(text="Show technical details")
        if self.on_layout_change is not None:
            self.on_layout_change()
