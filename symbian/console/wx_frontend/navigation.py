"""Contemporary desktop sidebar navigation for the console."""

from collections.abc import Callable

import wx

SIDEBAR = "#e9f4f7"
CONTENT = "#f5f5f7"
SELECTED = "#1269d3"
TEXT = "#20242b"
MUTED = "#586475"

_ROWS = (
    ("applications", "Applications", None),
    ("firmware", "Firmware", None),
    ("emulator", "Emulator", None),
    ("sdk", "SDK tools", None),
    ("sdk_setup", "Setup", "sdk"),
    ("sdk_inspection", "Inspection", "sdk"),
    ("sdk_preservation", "Preservation", "sdk"),
    ("devices", "Devices", None),
    ("device_actions", "Actions", "devices"),
    ("usb", "USB inspector", "devices"),
    ("protocols", "Protocols", "devices"),
    ("activity", "Activity", None),
)

_PARENT_PAGE = {"sdk": "sdk_setup", "devices": "device_actions"}


class NavigationItem(wx.Panel):
    """Paint a rounded macOS-style sidebar selection with keyboard access."""

    def __init__(
        self,
        parent: wx.Window,
        key: str,
        label: str,
        child: bool,
        selected: Callable[[str], None],
    ) -> None:
        super().__init__(parent, style=wx.WANTS_CHARS | wx.BORDER_NONE)
        self.key = key
        self.label = label
        self.child = child
        self.active = False
        self.emphasis = False
        self._selected = selected
        self.SetMinSize((-1, 37 if not child else 33))
        self.SetBackgroundStyle(wx.BG_STYLE_PAINT)
        self.SetCursor(wx.Cursor(wx.CURSOR_HAND))
        self.SetName(label)
        self.Bind(wx.EVT_PAINT, self._paint)
        self.Bind(wx.EVT_LEFT_DOWN, self._click)
        self.Bind(wx.EVT_KEY_DOWN, self._key)

    def _paint(self, _event: wx.PaintEvent) -> None:
        surface = wx.AutoBufferedPaintDC(self)
        surface.SetBackground(wx.Brush(wx.Colour(SIDEBAR)))
        surface.Clear()
        context = wx.GraphicsContext.Create(surface)
        if self.active:
            context.SetBrush(wx.Brush(wx.Colour(SELECTED)))
            context.SetPen(wx.TRANSPARENT_PEN)
            context.DrawRoundedRectangle(
                5,
                2,
                self.GetClientSize().width - 10,
                self.GetClientSize().height - 4,
                9,
            )
        font = wx.SystemSettings.GetFont(wx.SYS_DEFAULT_GUI_FONT)
        font.SetPointSize(11 if self.child else 12)
        font.SetWeight(
            wx.FONTWEIGHT_BOLD
            if self.active or self.emphasis
            else wx.FONTWEIGHT_NORMAL
        )
        colour = wx.WHITE if self.active else wx.Colour(TEXT)
        context.SetFont(font, colour)
        width, height = context.GetTextExtent(self.label)
        context.DrawText(
            self.label,
            35 if self.child else 16,
            (self.GetClientSize().height - height) / 2,
        )

    def _click(self, _event: wx.MouseEvent) -> None:
        self.SetFocus()
        self._selected(self.key)

    def _key(self, event: wx.KeyEvent) -> None:
        if event.GetKeyCode() in (wx.WXK_RETURN, wx.WXK_SPACE):
            self._selected(self.key)
        else:
            event.Skip()


class NavigationSidebar(wx.Panel):
    """Show only the relevant child sections beside persistent context."""

    def __init__(
        self,
        parent: wx.Window,
        selected: Callable[[str], None],
        context_panel: wx.Window,
    ) -> None:
        super().__init__(parent)
        self.SetMinSize((244, -1))
        self.SetBackgroundColour(wx.Colour(SIDEBAR))
        self._selected = selected
        self._rows: dict[str, NavigationItem] = {}
        self._group = "applications"
        layout = wx.BoxSizer(wx.VERTICAL)
        title = wx.StaticText(self, label="Symbian Console")
        font = title.GetFont()
        font.SetPointSize(15)
        font.SetWeight(wx.FONTWEIGHT_BOLD)
        title.SetFont(font)
        title.SetForegroundColour(wx.Colour(TEXT))
        layout.Add(title, 0, wx.LEFT | wx.TOP | wx.BOTTOM, 16)
        self.scroll = wx.ScrolledWindow(self, style=wx.VSCROLL | wx.BORDER_NONE)
        self.scroll.SetBackgroundColour(wx.Colour(SIDEBAR))
        self.scroll.SetScrollRate(0, 12)
        rows = wx.BoxSizer(wx.VERTICAL)
        for key, label, parent_key in _ROWS:
            item = NavigationItem(
                self.scroll,
                key,
                label,
                parent_key is not None,
                self._activate,
            )
            rows.Add(item, 0, wx.EXPAND | wx.LEFT | wx.RIGHT, 8)
            self._rows[key] = item
        self.scroll.SetSizer(rows)
        layout.Add(self.scroll, 1, wx.EXPAND | wx.BOTTOM, 8)
        separator = wx.StaticLine(self)
        layout.Add(separator, 0, wx.EXPAND | wx.LEFT | wx.RIGHT, 14)
        context_panel.Reparent(self)
        layout.Add(context_panel, 0, wx.EXPAND | wx.BOTTOM, 8)
        self.SetSizer(layout)
        self.select("applications")

    def _activate(self, key: str) -> None:
        page = _PARENT_PAGE.get(key, key)
        self.select(page)
        self._selected(page)

    def select(self, page: str) -> None:
        """Select a page and expand only its parent's child rows."""
        group = next(
            (parent for key, _, parent in _ROWS if key == page and parent),
            page,
        )
        self._group = group
        for key, _, parent in _ROWS:
            item = self._rows[key]
            item.Show(parent is None or parent == group)
            item.active = key == page
            item.emphasis = key == group and key != page
            item.Refresh()
        self.scroll.Layout()
        self.scroll.FitInside()
        self.Layout()
