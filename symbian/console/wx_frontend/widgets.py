"""Reusable native controls for technical and task results."""

import json
from typing import Any

import wx
import wx.stc

from symbian.console.models import OutcomeSummary
from symbian.console.syntax import LANGUAGES, highlighted_spans
from symbian.console.wx_frontend.navigation import CONTENT, MUTED


def heading(parent: wx.Window, text: str, size: int = 20) -> wx.StaticText:
    """Create a system-font section heading."""
    control = wx.StaticText(parent, label=text)
    font = control.GetFont()
    font.SetPointSize(size)
    font.SetWeight(wx.FONTWEIGHT_BOLD)
    control.SetFont(font)
    return control


def note(parent: wx.Window, text: str) -> wx.StaticText:
    """Create compact native explanatory text."""
    control = wx.StaticText(parent, label=text)
    control.SetForegroundColour(wx.Colour(MUTED))
    control.Wrap(680)
    return control


class RoundedCard(wx.Panel):
    """A light grouped surface surrounding native child controls."""

    def __init__(self, parent: wx.Window) -> None:
        super().__init__(parent, style=wx.BORDER_NONE)
        self.SetBackgroundStyle(wx.BG_STYLE_PAINT)
        self.content = wx.Panel(self)
        self.content.SetBackgroundColour(wx.WHITE)
        layout = wx.BoxSizer(wx.VERTICAL)
        layout.Add(self.content, 1, wx.EXPAND | wx.ALL, 8)
        self.SetSizer(layout)
        self.Bind(wx.EVT_PAINT, self._paint)

    def _paint(self, _event: wx.PaintEvent) -> None:
        surface = wx.AutoBufferedPaintDC(self)
        surface.SetBackground(wx.Brush(wx.Colour(CONTENT)))
        surface.Clear()
        context = wx.GraphicsContext.Create(surface)
        context.SetBrush(wx.Brush(wx.WHITE))
        context.SetPen(wx.Pen(wx.Colour("#dfe3e8"), 1))
        size = self.GetClientSize()
        context.DrawRoundedRectangle(
            0.5, 0.5, max(size.width - 1, 0), max(size.height - 1, 0), 11
        )


class ResultPanel(wx.Panel):
    """Show useful facts first and technical text on demand."""

    def __init__(self, parent: wx.Window) -> None:
        super().__init__(parent)
        self.SetBackgroundColour(wx.WHITE)
        self._payload: Any = None
        self._expanded = False
        layout = wx.BoxSizer(wx.VERTICAL)
        self.title = heading(self, "", 13)
        layout.Add(self.title, 0, wx.BOTTOM, 4)
        self.message = note(self, "")
        layout.Add(self.message, 0, wx.BOTTOM, 8)
        self.facts = wx.StaticText(self, label="")
        layout.Add(self.facts, 0, wx.BOTTOM, 8)
        controls = wx.BoxSizer(wx.HORIZONTAL)
        self.toggle = wx.Button(self, label="Show technical details")
        self.toggle.Bind(wx.EVT_BUTTON, self._toggle)
        controls.Add(self.toggle, 0, wx.RIGHT, 8)
        self.language = wx.Choice(self, choices=list(LANGUAGES))
        self.language.SetStringSelection("JSON")
        self.language.Bind(wx.EVT_CHOICE, lambda event: self._render())
        controls.Add(self.language, 0, wx.RIGHT, 8)
        self.copy = wx.Button(self, label="Copy")
        self.copy.Bind(wx.EVT_BUTTON, self._copy)
        controls.Add(self.copy)
        layout.Add(controls, 0, wx.BOTTOM, 8)
        self.code = wx.stc.StyledTextCtrl(
            self, style=wx.BORDER_SIMPLE, size=(-1, 240)
        )
        self.code.SetReadOnly(True)
        self.code.SetWrapMode(wx.stc.STC_WRAP_NONE)
        self.code.StyleSetFont(
            0, wx.Font(wx.FontInfo(11).Family(wx.FONTFAMILY_TELETYPE))
        )
        colours = {
            "plain": "#20242b",
            "comment": "#64748b",
            "error": "#ad253e",
            "string": "#16804a",
            "number": "#a35200",
            "keyword": "#6b46b0",
            "tag": "#075da8",
            "attribute": "#075da8",
            "operator": "#5c6470",
        }
        self._styles = {name: index + 1 for index, name in enumerate(colours)}
        for name, colour in colours.items():
            self.code.StyleSetForeground(self._styles[name], wx.Colour(colour))
        layout.Add(self.code, 0, wx.EXPAND)
        self.code.Hide()
        self.SetSizer(layout)
        self.Hide()

    def show(self, summary: OutcomeSummary, payload: Any) -> None:
        """Present a result while retaining the previous disclosure state."""
        self._payload = payload
        self.title.SetLabel(summary.title)
        self.message.SetLabel(summary.message)
        self.facts.SetLabel(
            "\n".join(f"{fact.label}:  {fact.value}" for fact in summary.facts)
        )
        self._render()
        self.Show()
        self._reflow()

    def _toggle(self, _event: wx.CommandEvent) -> None:
        self._expanded = not self._expanded
        self.code.Show(self._expanded)
        self.toggle.SetLabel(
            "Hide technical details"
            if self._expanded
            else "Show technical details"
        )
        self._reflow()

    def _render(self) -> None:
        language = self.language.GetStringSelection()
        if isinstance(self._payload, str) and language == "Plain text":
            rendered = self._payload
        else:
            rendered = json.dumps(
                self._payload, indent=2, ensure_ascii=False, default=str
            )
        self.code.SetReadOnly(False)
        self.code.SetText(rendered)
        self.code.StartStyling(0)
        for category, fragment in highlighted_spans(rendered, language):
            self.code.SetStyling(
                len(fragment.encode("utf-8")), self._styles[category]
            )
        self.code.SetReadOnly(True)

    def _copy(self, _event: wx.CommandEvent) -> None:
        if wx.TheClipboard.Open():
            wx.TheClipboard.SetData(wx.TextDataObject(self.code.GetText()))
            wx.TheClipboard.Close()

    def _reflow(self) -> None:
        parent = self.GetParent()
        while parent is not None:
            parent.Layout()
            if isinstance(parent, wx.ScrolledWindow):
                parent.FitInside()
                break
            parent = parent.GetParent()
