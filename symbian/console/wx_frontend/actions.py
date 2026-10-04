"""Native guided forms for catalogued SDK operations."""

from collections.abc import Callable
from pathlib import Path
from typing import Any

import wx

from symbian.console.arguments import argument_tokens
from symbian.console.client import ConsoleClient
from symbian.console.defaults import initial_value
from symbian.console.models import (
    CommandArgument,
    CommandCatalog,
    CommandRequest,
    CommandResult,
    CommandSpec,
    ConsoleContext,
    OutcomeSummary,
)
from symbian.console.presentation import (
    PRESENTATIONS,
    TaskStep,
    summarize_command,
    task_steps,
)
from symbian.console.selection import device_labels
from symbian.console.validation import field_label, validate_task
from symbian.console.wx_frontend.navigation import CONTENT
from symbian.console.wx_frontend.widgets import (
    ResultPanel,
    RoundedCard,
    heading,
    note,
)
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

_ADVANCED = {
    "at_status",
    "backend",
    "clear_firmware",
    "compiler",
    "emulator",
    "gdb",
    "headers",
    "import_proxy",
    "importer",
    "language",
    "linker",
    "mtp",
    "mtp_list",
    "no_build",
    "no_protocol",
    "non_interactive",
    "oracles_build",
    "output",
    "portable_runtime",
    "profile",
    "replace_alias",
    "root",
    "rpkg",
    "saved",
    "store",
    "timeout",
    "unset",
    "usb_map",
    "variant",
    "volume",
    "z_drive",
}
_PATHS = {
    "destination",
    "project",
    "sdk",
    "store",
    "workspace",
    "output",
    "artifact",
    "package",
    "executable",
    "source",
    "rom",
    "vpl",
    "instance",
    "bundle",
    "rpkg",
    "z_drive",
    "archive",
    "ticket",
    "headers",
    "sources_root",
    "oracles_build",
    "definition",
    "elf",
    "import_proxy",
    "root",
    "firmware",
    "manifest",
    "manifest_sha256",
}


class ActionPanel(wx.Panel):
    """Keep a task list visible beside a single scrollable wizard page."""

    def __init__(
        self,
        parent: wx.Window,
        client: ConsoleClient,
        submit: Submit,
        heading_text: str,
        summary: str,
        groups: tuple[str, ...],
    ) -> None:
        super().__init__(parent)
        self._client = client
        self._submit = submit
        self._groups = groups
        self._catalog: tuple[CommandSpec, ...] = ()
        self._context: ConsoleContext | None = None
        self._selected_device: str | None = None
        self._task: CommandSpec | None = None
        self._step_index = 0
        self._drafts: dict[tuple[str, ...], dict[str, str | bool]] = {}
        self._outcomes: dict[tuple[str, ...], tuple[OutcomeSummary, Any]] = {}
        self._controls: dict[str, wx.Window] = {}
        self._advanced = False
        self._configure_more = False
        self.SetBackgroundColour(wx.Colour(CONTENT))

        outer = wx.BoxSizer(wx.VERTICAL)
        outer.Add(
            heading(self, heading_text), 0, wx.LEFT | wx.RIGHT | wx.TOP, 22
        )
        outer.Add(
            note(self, summary), 0, wx.LEFT | wx.RIGHT | wx.TOP | wx.BOTTOM, 22
        )
        content = wx.BoxSizer(wx.HORIZONTAL)
        navigation = RoundedCard(self)
        navigation.SetMinSize((255, -1))
        navigation_content = navigation.content
        navigation_sizer = wx.BoxSizer(wx.VERTICAL)
        navigation_sizer.Add(
            heading(navigation_content, "Actions", 12),
            0,
            wx.LEFT | wx.RIGHT | wx.TOP | wx.BOTTOM,
            10,
        )
        self.actions = wx.ListBox(
            navigation_content,
            style=wx.LB_SINGLE | wx.BORDER_NONE,
            size=(-1, 220),
        )
        self.actions.Bind(wx.EVT_LISTBOX, self._select_action)
        navigation_sizer.Add(
            self.actions, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM, 8
        )
        navigation_content.SetSizer(navigation_sizer)
        content.Add(navigation, 0, wx.ALIGN_TOP | wx.LEFT, 22)
        content.AddSpacer(18)

        self.scroll = wx.ScrolledWindow(
            self, style=wx.VSCROLL | wx.TAB_TRAVERSAL
        )
        self.scroll.SetBackgroundColour(wx.Colour(CONTENT))
        self.scroll.SetScrollRate(0, 12)
        self.scroll_sizer = wx.BoxSizer(wx.VERTICAL)
        self.card = RoundedCard(self.scroll)
        card_sizer = wx.BoxSizer(wx.VERTICAL)
        self.form_host = wx.Panel(self.card.content)
        self.form_host.SetBackgroundColour(wx.WHITE)
        self.form_sizer = wx.BoxSizer(wx.VERTICAL)
        self.form_host.SetSizer(self.form_sizer)
        card_sizer.Add(
            self.form_host, 0, wx.EXPAND | wx.LEFT | wx.RIGHT | wx.TOP, 16
        )
        self.result = ResultPanel(self.card.content)
        card_sizer.Add(
            self.result,
            0,
            wx.EXPAND | wx.LEFT | wx.RIGHT | wx.BOTTOM | wx.TOP,
            16,
        )
        self.card.content.SetSizer(card_sizer)
        self.scroll_sizer.Add(
            self.card, 0, wx.EXPAND | wx.RIGHT | wx.BOTTOM, 16
        )
        self.scroll.SetSizer(self.scroll_sizer)
        content.Add(self.scroll, 1, wx.EXPAND | wx.RIGHT | wx.BOTTOM, 22)
        outer.Add(content, 1, wx.EXPAND)
        self.SetSizer(outer)
        self.Bind(wx.EVT_SIZE, self._sized)

    def _sized(self, event: wx.SizeEvent) -> None:
        self.scroll.FitInside()
        event.Skip()

    def set_catalog(self, catalog: CommandCatalog) -> None:
        """Keep only actions belonging to this navigation section."""
        self._catalog = tuple(
            specification
            for specification in catalog.commands
            if PRESENTATIONS.get(specification.path) is not None
            and PRESENTATIONS[specification.path].group in self._groups
        )
        self.actions.Set(
            [PRESENTATIONS[item.path].title for item in self._catalog]
        )
        self.actions.SetMinSize((225, max(100, len(self._catalog) * 27 + 10)))
        self.actions.GetParent().Layout()
        if self._catalog:
            self.actions.SetSelection(0)
            self._choose(self._catalog[0])

    def set_context(self, context: ConsoleContext) -> None:
        """Update actual path and device defaults without discarding edits."""
        self._context = context
        if self._task is not None:
            self._save_controls()
            self._render()

    def select_device(self, selector: str | None) -> None:
        """Synchronize the selected live phone with device fields."""
        self._selected_device = selector
        if self._task is not None and any(
            argument.name == "device" for argument in self._task.arguments
        ):
            self._save_controls()
            self._drafts[self._task.path]["device"] = selector or ""
            self._render()

    def _select_action(self, _event: wx.CommandEvent) -> None:
        if self._task is not None:
            self._save_controls()
        index = self.actions.GetSelection()
        if index != wx.NOT_FOUND:
            self._choose(self._catalog[index])

    def _choose(self, specification: CommandSpec) -> None:
        self._task = specification
        self._step_index = 0
        self._advanced = False
        self._configure_more = False
        self._drafts.setdefault(specification.path, {})
        if specification.path in self._outcomes:
            summary, payload = self._outcomes[specification.path]
            self.result.show(summary, payload)
        else:
            self.result.Hide()
        self._render()

    def _value(self, argument: CommandArgument) -> str | bool:
        assert self._task is not None
        draft = self._drafts[self._task.path]
        if argument.name in draft:
            return draft[argument.name]
        return initial_value(
            self._context, self._task.path, argument, self._selected_device
        )

    def _save_controls(self) -> None:
        if self._task is None:
            return
        draft = self._drafts[self._task.path]
        for name, control in self._controls.items():
            if isinstance(control, wx.CheckBox):
                draft[name] = control.GetValue()
            elif isinstance(control, wx.Choice):
                selection = control.GetStringSelection()
                draft[name] = (
                    self._device_choices.get(selection, "")
                    if name == "device"
                    else selection
                )
            elif isinstance(control, wx.TextCtrl):
                draft[name] = control.GetValue()

    def _add_field(self, argument: CommandArgument) -> None:
        assert self._task is not None
        label = field_label(self._task.path, argument)
        if argument.required:
            label += " · Required"
        self.form_sizer.Add(
            wx.StaticText(self.form_host, label=label), 0, wx.TOP, 10
        )
        value = self._value(argument)
        if argument.kind == "flag":
            control = wx.CheckBox(
                self.form_host, label=argument.help or "Enable"
            )
            control.SetValue(bool(value))
            self.form_sizer.Add(control, 0, wx.TOP, 4)
        elif argument.name == "device" and self._context is not None:
            labels = device_labels(self._context.devices)
            control = wx.Choice(self.form_host, choices=list(labels))
            self._device_choices = labels
            for index, selector in enumerate(labels.values()):
                if selector == value:
                    control.SetSelection(index)
                    break
            self.form_sizer.Add(control, 0, wx.EXPAND | wx.TOP, 4)
        elif argument.choices:
            control = wx.Choice(self.form_host, choices=list(argument.choices))
            if value in argument.choices:
                control.SetStringSelection(str(value))
            self.form_sizer.Add(control, 0, wx.EXPAND | wx.TOP, 4)
        else:
            row = wx.BoxSizer(wx.HORIZONTAL)
            control = wx.TextCtrl(
                self.form_host, value=str(value), size=(-1, 27)
            )
            row.Add(control, 1, wx.EXPAND)
            if argument.name in _PATHS and argument.name != "manifest_sha256":
                browse = wx.Button(self.form_host, label="Browse…")
                browse.Bind(
                    wx.EVT_BUTTON,
                    lambda event, field=argument, entry=control: self._browse(
                        field, entry
                    ),
                )
                row.Add(browse, 0, wx.LEFT, 8)
            self.form_sizer.Add(row, 0, wx.EXPAND | wx.TOP, 4)
        self._controls[argument.name] = control
        if argument.help and argument.kind != "flag":
            self.form_sizer.Add(
                note(self.form_host, argument.help), 0, wx.TOP, 3
            )

    def _browse(self, argument: CommandArgument, entry: wx.TextCtrl) -> None:
        current = entry.GetValue()
        folder = argument.name in {
            "project",
            "store",
            "workspace",
            "output",
            "archive",
            "headers",
            "sources_root",
            "oracles_build",
            "root",
            "instance",
        }
        if folder:
            with wx.DirDialog(
                self,
                "Choose a folder",
                defaultPath=current if Path(current).is_dir() else "",
            ) as dialog:
                if dialog.ShowModal() == wx.ID_OK:
                    entry.SetValue(dialog.GetPath())
        else:
            with wx.FileDialog(
                self, "Choose a file", defaultFile=Path(current).name
            ) as dialog:
                if dialog.ShowModal() == wx.ID_OK:
                    entry.SetValue(dialog.GetPath())

    def _render(self) -> None:
        if self._task is None:
            return
        self._controls = {}
        self.form_sizer.Clear(delete_windows=True)
        presentation = PRESENTATIONS[self._task.path]
        self.form_sizer.Add(
            heading(self.form_host, presentation.title), 0, wx.BOTTOM, 4
        )
        self.form_sizer.Add(
            note(self.form_host, presentation.summary), 0, wx.BOTTOM, 10
        )
        steps = self._steps()
        self._step_index = min(self._step_index, len(steps))
        if self._step_index == len(steps):
            self.form_sizer.Add(
                heading(self.form_host, "Review", 12), 0, wx.BOTTOM, 5
            )
            values = self._all_values()
            displayed = [
                f"{field_label(self._task.path, argument)}:  "
                f"{values.get(argument.name)}"
                for argument in self._task.arguments
                if values.get(argument.name) not in ("", False, None)
            ]
            self.form_sizer.Add(
                note(
                    self.form_host,
                    "\n".join(displayed) or "The SDK defaults will be used.",
                ),
                0,
                wx.BOTTOM,
                10,
            )
        else:
            step = steps[self._step_index]
            if len(steps) > 1:
                self.form_sizer.Add(
                    note(
                        self.form_host,
                        f"{self._step_index + 1} of {len(steps)} · "
                        f"{step.title}",
                    ),
                    0,
                    wx.BOTTOM,
                    5,
                )
            visible = [
                argument
                for argument in step.arguments
                if argument.required
                or argument.name not in _ADVANCED
                or self._advanced
            ]
            for argument in visible:
                self._add_field(argument)
            advanced_count = len(step.arguments) - len(visible)
            if advanced_count or self._advanced:
                toggle = wx.Button(
                    self.form_host,
                    label=(
                        "Hide additional settings"
                        if self._advanced
                        else f"Additional settings ({advanced_count})"
                    ),
                )
                toggle.Bind(wx.EVT_BUTTON, self._toggle_advanced)
                self.form_sizer.Add(toggle, 0, wx.TOP, 13)
        self.feedback = note(self.form_host, "")
        self.form_sizer.Add(self.feedback, 0, wx.TOP | wx.BOTTOM, 8)
        buttons = wx.BoxSizer(wx.HORIZONTAL)
        if self._step_index > 0:
            back = wx.Button(self.form_host, label="Back")
            back.Bind(wx.EVT_BUTTON, self._back)
            buttons.Add(back, 0, wx.RIGHT, 8)
        label = (
            presentation.action
            if self._step_index == len(steps)
            or (len(steps) == 1 and not self._task.arguments)
            else "Review" if self._step_index == len(steps) - 1 else "Continue"
        )
        forward = wx.Button(self.form_host, label=label)
        forward.Bind(wx.EVT_BUTTON, self._forward)
        buttons.Add(forward)
        if not self._configure_more and any(
            step.optional for step in task_steps(self._task)
        ):
            configure = wx.Button(self.form_host, label="Optional settings…")
            configure.Bind(wx.EVT_BUTTON, self._show_optional)
            buttons.Add(configure, 0, wx.LEFT, 12)
        self.form_sizer.Add(buttons, 0, wx.TOP, 6)
        self.form_host.Layout()
        self.card.content.Layout()
        self.card.Layout()
        self.scroll.Layout()
        self.scroll.FitInside()
        self.scroll.Scroll(0, 0)

    def _toggle_advanced(self, _event: wx.CommandEvent) -> None:
        self._save_controls()
        self._advanced = not self._advanced
        self._render()

    def _show_optional(self, _event: wx.CommandEvent) -> None:
        self._save_controls()
        self._configure_more = True
        self._step_index = 1
        self._render()

    def _steps(self) -> tuple[TaskStep, ...]:
        """Omit optional pages until the user chooses extra configuration."""
        assert self._task is not None
        steps = task_steps(self._task)
        if self._configure_more:
            return steps
        return tuple(step for step in steps if not step.optional)

    def _all_values(self) -> dict[str, str | bool]:
        assert self._task is not None
        return {
            argument.name: self._value(argument)
            for argument in self._task.arguments
        }

    def _back(self, _event: wx.CommandEvent) -> None:
        self._save_controls()
        self._step_index -= 1
        self._render()

    def _forward(self, _event: wx.CommandEvent) -> None:
        assert self._task is not None
        self._save_controls()
        steps = self._steps()
        if self._step_index < len(steps):
            current_names = {
                item.name for item in steps[self._step_index].arguments
            }
            issues = [
                issue
                for issue in validate_task(self._task, self._all_values())
                if issue.field in current_names
            ]
            if issues:
                self.feedback.SetLabel(issues[0].message)
                return
            if len(steps) == 1 and not self._task.arguments:
                self._run()
                return
            self._step_index += 1
            self._render()
            return
        issues = validate_task(self._task, self._all_values())
        if issues:
            self.feedback.SetLabel(issues[0].message)
            return
        self._run()

    def _run(self) -> None:
        assert self._task is not None
        request = CommandRequest(
            path=self._task.path,
            argv=argument_tokens(self._task, self._all_values()),
        )
        self.feedback.SetLabel("Working…")
        self._submit(
            lambda: self._client.run(request),
            lambda value, error, path=request.path: self._completed(
                path, value, error
            ),
            PRESENTATIONS[self._task.path].title,
            self._task.path if self._task.path[0] == "device" else None,
        )

    def _completed(
        self, path: tuple[str, ...], value: Any, error: Exception | None
    ) -> None:
        if error is not None:
            status = Status.from_exception(error)
            summary = OutcomeSummary(title="Task failed", message=str(status))
            payload = status.model_dump(mode="json")
        else:
            result: CommandResult = value
            summary = summarize_command(result)
            payload = result.result
        self._outcomes[path] = (summary, payload)
        if self._task is not None and self._task.path == path:
            self.feedback.SetLabel("")
            self.result.show(summary, payload)
            self.scroll.FitInside()
