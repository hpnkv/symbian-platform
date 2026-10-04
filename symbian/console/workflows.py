"""Guided task views for the SDK's public CLI operations."""

import tkinter as tk
from collections.abc import Callable
from tkinter import filedialog, ttk
from typing import Any

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
    TaskPresentation,
    TaskStep,
    summarize_command,
    task_steps,
)
from symbian.console.selection import device_labels
from symbian.console.validation import (
    field_label,
    validate_field,
    validate_task,
)
from symbian.console.widgets import (
    OutcomePanel,
    configure_tree_rows,
    title,
)
from symbian.status import Status

Submit = Callable[
    [Callable[[], Any], Callable[[Any, Exception | None], None], str], None
]

_DIRECTORY_FIELDS = {
    "project",
    "sdk",
    "workspace",
    "store",
    "root",
    "oracles_build",
    "sources_root",
    "headers",
    "z_drive",
    "instance",
    "volume",
    "output",
}
_FILE_FIELDS = {
    "artifact",
    "package",
    "executable",
    "definition",
    "elf",
    "profile",
    "rom",
    "vpl",
    "rpkg",
    "bundle",
    "source",
    "import_proxy",
    "emulator",
    "importer",
    "compiler",
    "linker",
    "gdb",
    "endpoint",
}
_FIRMWARE_SOURCES = (
    ("Archive", "source"),
    ("ROM image", "rom"),
    ("VPL manifest", "vpl"),
    ("Existing emulator instance", "instance"),
    ("Portable bundle", "bundle"),
)
_ADVANCED_FIELDS = {
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
    "workspace",
    "z_drive",
    "obex_connect",
}


class WorkflowPanel(ttk.Frame):
    """Present every CLI operation as a named task with guided steps."""

    def __init__(
        self,
        parent: tk.Misc,
        client: ConsoleClient,
        submit: Submit,
        on_device_selected: Callable[[str], None] | None = None,
        *,
        heading: str,
        summary: str,
        groups: tuple[str, ...],
    ) -> None:
        super().__init__(parent, padding=18)
        self._client = client
        self._submit = submit
        self._on_device_selected = on_device_selected
        self.on_outer_wheel: Callable[[int], None] | None = None
        self.on_layout_change: Callable[[], None] | None = None
        self._groups = frozenset(groups)
        self._context: ConsoleContext | None = None
        self._selected_device: str | None = None
        self._device_labels: dict[str, str] = {}
        self._edited: set[tuple[tuple[str, ...], str]] = set()
        self._specs: tuple[CommandSpec, ...] = ()
        self._visible: list[CommandSpec] = []
        self._task: CommandSpec | None = None
        self._steps: tuple[TaskStep, ...] = ()
        self._step_index = 0
        self._include_optional = False
        self._drafts: dict[tuple[str, ...], dict[str, str | bool]] = {}
        self._variables: dict[str, tk.Variable] = {}
        self._errors: dict[str, ttk.Label] = {}
        self._help_labels: list[ttk.Label] = []
        self._review_labels: list[ttk.Label] = []
        self._current_arguments: tuple[CommandArgument, ...] = ()
        self._advanced_open: dict[tuple[tuple[str, ...], int], bool] = {}
        title(
            self,
            heading,
            summary,
        ).pack(fill="x", pady=(0, 16))
        content = ttk.Frame(self)
        content.pack(fill="both", expand=True)
        content.grid_columnconfigure(0, minsize=250, weight=1)
        content.grid_columnconfigure(1, weight=3)
        content.grid_rowconfigure(0, weight=1)

        browser = ttk.Frame(content)
        browser.grid(row=0, column=0, sticky="nsew", padx=(0, 16))
        ttk.Label(browser, text="Actions", style="Section.TLabel").pack(
            anchor="w", pady=(0, 5)
        )
        self.search_section = ttk.Frame(browser)
        self.search_section.pack(fill="x")
        ttk.Label(
            self.search_section, text="Find an action", style="Muted.TLabel"
        ).pack(anchor="w", pady=(0, 4))
        self.search = tk.StringVar()
        ttk.Entry(self.search_section, textvariable=self.search).pack(
            fill="x", pady=(0, 9)
        )
        self.search.trace_add("write", lambda *_args: self._filter())
        self.tasks = ttk.Treeview(browser, show="tree", selectmode="browse")
        configure_tree_rows(self.tasks)
        self.tasks.pack(fill="both", expand=True)
        self.tasks.bind("<<TreeviewSelect>>", self._select)

        detail = ttk.Frame(content)
        detail.grid(row=0, column=1, sticky="nsew")
        detail.bind("<Configure>", self._resize_detail)
        self.heading = ttk.Label(
            detail, text="Select a task", style="Title.TLabel"
        )
        self.heading.pack(anchor="w")
        self.description = ttk.Label(
            detail,
            text="Task guidance will appear here.",
            style="Muted.TLabel",
            wraplength=680,
        )
        self.description.pack(anchor="w", pady=(4, 11))
        ttk.Separator(detail).pack(fill="x", pady=(0, 10))
        self.progress = ttk.Label(detail, text="", style="Muted.TLabel")
        self.progress.pack(anchor="w", pady=(0, 7))
        self.step_heading = ttk.Label(detail, text="", style="Section.TLabel")
        self.step_heading.pack(anchor="w", pady=(0, 3))
        self.guidance = ttk.Label(
            detail,
            text="",
            style="Muted.TLabel",
            wraplength=680,
        )
        self.guidance.pack(anchor="w", pady=(0, 9))
        self.form_container = ttk.Frame(detail)
        self.form_container.pack(fill="x")
        self.form = ttk.Frame(self.form_container)
        self.form.pack(fill="x")
        for sequence in ("<MouseWheel>", "<Button-4>", "<Button-5>"):
            self.form.bind(sequence, self._form_mousewheel, add="+")
        actions = ttk.Frame(detail)
        self.actions = actions
        actions.pack(fill="x", pady=(10, 11))
        self.back_button = ttk.Button(actions, text="Back", command=self._back)
        self.back_button.pack(side="left")
        self.next_button = ttk.Button(
            actions,
            text="Continue",
            style="Accent.TButton",
            command=self._next_or_run,
        )
        self.next_button.pack(side="left", padx=(8, 0))
        self.more_button = ttk.Button(
            actions,
            text="Optional settings…",
            command=self._configure_more,
        )
        self.feedback = ttk.Label(actions, text="", style="Muted.TLabel")
        self.feedback.pack(side="left", padx=12)
        self.outcome = OutcomePanel(detail)

    def set_catalog(self, catalog: CommandCatalog) -> None:
        """Refresh available task definitions from the canonical CLI."""
        self._specs = tuple(
            spec
            for spec in catalog.commands
            if self._presentation(spec).group in self._groups
        )
        if len(self._specs) < 8:
            self.search_section.pack_forget()
        self._filter()

    def set_context(self, context: ConsoleContext) -> None:
        """Populate actual current paths and available phone selectors."""
        previous = self._context
        previous_devices = (
            tuple(device.selector for device in previous.devices)
            if previous is not None
            else ()
        )
        devices_changed = previous_devices != tuple(
            device.selector for device in context.devices
        )
        paths_changed = previous is None or (
            previous.workspace,
            previous.project,
            previous.sdk_manifest,
            previous.firmware_store,
        ) != (
            context.workspace,
            context.project,
            context.sdk_manifest,
            context.firmware_store,
        )
        self._save_current()
        self._context = context
        self._device_labels = device_labels(context.devices)
        for path, values in self._drafts.items():
            for name in tuple(values):
                if name == "device" and values[name] not in {
                    device.selector for device in context.devices
                }:
                    values.pop(name)
                    continue
                if values[name] == "" and (path, name) not in self._edited:
                    values.pop(name)
        device_task = self._task is not None and any(
            argument.name == "device" for argument in self._task.arguments
        )
        if self._task is not None and (
            paths_changed or (devices_changed and device_task)
        ):
            self._render_step()

    def select_device(self, selector: str | None) -> None:
        """Follow the selected phone across task and inspection views."""
        self._selected_device = selector
        if self._task is None:
            return
        if any(arg.name == "device" for arg in self._task.arguments):
            if selector is None:
                self._drafts.setdefault(self._task.path, {}).pop("device", None)
            else:
                self._drafts.setdefault(self._task.path, {})[
                    "device"
                ] = selector
            variable = self._variables.get("device")
            if variable is not None:
                label = next(
                    (
                        label
                        for label, value in self._device_labels.items()
                        if value == selector
                    ),
                    selector or "",
                )
                if variable.get() != label:
                    variable.set(label)

    def select_task(self, path: tuple[str, ...]) -> None:
        """Open a named task within this section."""
        specification = next(
            (spec for spec in self._specs if spec.path == path), None
        )
        if specification is None:
            return
        self.search.set("")
        self._filter()
        item = " ".join(path)
        if self.tasks.exists(item):
            self.tasks.selection_set(item)
            self.tasks.see(item)
            self._select(None)

    def _presentation(self, spec: CommandSpec) -> TaskPresentation:
        return PRESENTATIONS.get(
            spec.path,
            TaskPresentation(
                path=spec.path,
                group="SDK and toolchain",
                title=spec.title.title(),
                summary=spec.description,
            ),
        )

    def _filter(self) -> None:
        self._save_current()
        query = self.search.get().casefold().strip()
        self._visible = [
            spec
            for spec in self._specs
            if (
                query in self._presentation(spec).title.casefold()
                or query in self._presentation(spec).summary.casefold()
            )
        ]
        self.tasks.delete(*self.tasks.get_children())
        for index, spec in enumerate(self._visible):
            presentation = self._presentation(spec)
            self.tasks.insert(
                "",
                "end",
                iid=" ".join(spec.path),
                text=presentation.title,
                tags=("odd" if index % 2 else "even",),
            )
        if self._visible:
            self.tasks.selection_set(" ".join(self._visible[0].path))
            self._select(None)

    def _save_current(self) -> None:
        if self._task is None:
            return
        draft = self._drafts.setdefault(self._task.path, {})
        for name, variable in self._variables.items():
            value = variable.get()
            draft[name] = (
                self._device_labels.get(value, value)
                if name == "device"
                else value
            )

    def _value_for(
        self,
        draft: dict[str, str | bool],
        argument: CommandArgument,
    ) -> str | bool:
        if argument.name in draft:
            return draft[argument.name]
        assert self._task is not None
        return initial_value(
            self._context,
            self._task.path,
            argument,
            self._selected_device,
        )

    def _select(self, _event: tk.Event | None) -> None:
        selected = self.tasks.selection()
        if not selected:
            return
        specification = next(
            (
                spec
                for spec in self._visible
                if " ".join(spec.path) == selected[0]
            ),
            None,
        )
        if specification is None:
            return
        if self._task is not None and self._task.path != specification.path:
            self._save_current()
        self._task = specification
        self._steps = (
            task_steps(specification) if specification.arguments else ()
        )
        self._drafts.setdefault(specification.path, {})
        self._step_index = 0
        self._include_optional = False
        presentation = self._presentation(specification)
        self.heading.configure(text=presentation.title)
        self.description.configure(text=presentation.summary)
        self.feedback.configure(text="")
        self.outcome.hide()
        self._render_step()

    def _render_step(self) -> None:
        for child in self.form.winfo_children():
            child.destroy()
        self._variables = {}
        self._errors = {}
        self._help_labels = []
        self._review_labels = []
        self.after_idle(self._bind_form_wheel)
        if self.on_layout_change is not None:
            self.after_idle(self.on_layout_change)
        if self._task is None:
            return
        direct = not self._steps
        review = self._step_index == len(self._steps)
        self.progress.configure(
            text=(
                self._presentation(self._task).group
                + "  ›  "
                + (
                    "Ready"
                    if direct
                    else (
                        "Review"
                        if review
                        else self._steps[self._step_index].title
                    )
                )
            )
        )
        self.step_heading.configure(
            text=(
                "Ready"
                if direct
                else "Review" if review else self._steps[self._step_index].title
            )
        )
        self.guidance.configure(
            text=(
                "This action needs no input. Run it when ready."
                if direct
                else (
                    "Check the information below before running this task."
                    if review
                    else self._steps[self._step_index].guidance
                )
            )
        )
        if direct:
            self.form_container.pack_forget()
            self.back_button.pack_forget()
        else:
            if not self.form_container.winfo_manager():
                self.form_container.pack(fill="x", before=self.actions)
            if not self.back_button.winfo_manager():
                self.back_button.pack(side="left", before=self.next_button)
        self.back_button.configure(
            state="normal" if self._step_index else "disabled"
        )
        self.next_button.configure(
            text=(
                self._presentation(self._task).action
                if review
                else "Review" if self._can_skip_remaining() else "Continue"
            )
        )
        self.more_button.pack_forget()
        if not review and self._can_skip_remaining():
            self.more_button.pack(side="left", padx=(8, 0))
        if review:
            if not direct:
                self._render_review()
            self._current_arguments = ()
            return
        self._current_arguments = self._steps[self._step_index].arguments
        draft = self._drafts.setdefault(self._task.path, {})
        if (
            self._task.path == ("firmware", "import")
            and self._steps[self._step_index].title == "Source"
        ):
            self._render_firmware_source(draft)
            return
        self._render_fields(draft)
        if not self._current_arguments:
            ttk.Label(
                self.form,
                text="No inputs are needed for this step.",
                style="Muted.TLabel",
            ).pack(anchor="w", pady=10)

    def _render_firmware_source(self, draft: dict[str, str | bool]) -> None:
        """Show only the selected firmware import source and its companions."""
        selected_name = str(draft.get("_source_kind", ""))
        if not selected_name:
            selected_name = next(
                (name for _, name in _FIRMWARE_SOURCES if draft.get(name)),
                "source",
            )
        selected_label = next(
            label for label, name in _FIRMWARE_SOURCES if name == selected_name
        )
        ttk.Label(self.form, text="Firmware source type").pack(
            anchor="w", pady=(0, 4)
        )
        source_type = tk.StringVar(value=selected_label)
        source_picker = ttk.Combobox(
            self.form,
            textvariable=source_type,
            values=tuple(label for label, _ in _FIRMWARE_SOURCES),
            state="readonly",
        )
        source_picker.pack(fill="x", pady=(0, 5))
        ttk.Label(
            self.form,
            text=(
                "Choose one source. ROM images may also use an RPKG "
                "or drive-Z companion."
            ),
            style="Muted.TLabel",
            wraplength=650,
        ).pack(anchor="w", pady=(0, 12))
        source_picker.bind(
            "<<ComboboxSelected>>",
            lambda _event: self._change_firmware_source(source_type.get()),
        )
        selected = tuple(
            argument
            for argument in self._current_arguments
            if argument.name == selected_name
            or (selected_name == "rom" and argument.name in {"rpkg", "z_drive"})
        )
        self._current_arguments = selected
        self._render_fields(draft)

    def _render_fields(self, draft: dict[str, str | bool]) -> None:
        """Keep routine inputs visible and disclose advanced settings."""
        assert self._task is not None
        core = tuple(
            argument
            for argument in self._current_arguments
            if argument.required or argument.name not in _ADVANCED_FIELDS
        )
        advanced = tuple(
            argument
            for argument in self._current_arguments
            if argument not in core
        )
        for argument in core:
            self._make_field(argument, self._value_for(draft, argument))
        if not advanced:
            return
        key = (self._task.path, self._step_index)
        expanded = self._advanced_open.get(key, False)
        ttk.Button(
            self.form,
            text=(
                f"Hide additional settings ({len(advanced)})"
                if expanded
                else f"Additional settings ({len(advanced)})…"
            ),
            command=self._toggle_advanced,
        ).pack(anchor="w", pady=(4, 10))
        if expanded:
            for argument in advanced:
                self._make_field(argument, self._value_for(draft, argument))

    def _toggle_advanced(self) -> None:
        assert self._task is not None
        self._save_current()
        key = (self._task.path, self._step_index)
        self._advanced_open[key] = not self._advanced_open.get(key, False)
        self._render_step()

    def _change_firmware_source(self, label: str) -> None:
        assert self._task is not None
        self._save_current()
        draft = self._drafts[self._task.path]
        for name in (
            "rpkg",
            "z_drive",
            *(name for _, name in _FIRMWARE_SOURCES),
        ):
            draft.pop(name, None)
        draft["_source_kind"] = next(
            name
            for item_label, name in _FIRMWARE_SOURCES
            if item_label == label
        )
        self._render_step()

    def _make_field(
        self, argument: CommandArgument, stored: str | bool
    ) -> None:
        assert self._task is not None
        row = ttk.Frame(self.form)
        row.pack(fill="x", pady=(0, 8))
        heading = field_label(self._task.path, argument)
        ttk.Label(
            row,
            text=heading + ("  ·  Required" if argument.required else ""),
        ).pack(anchor="w", pady=(0, 3))
        control_row = ttk.Frame(row)
        control_row.pack(fill="x")
        if argument.kind == "flag":
            variable: tk.Variable = tk.BooleanVar(value=bool(stored))
            ttk.Checkbutton(control_row, text="Enable", variable=variable).pack(
                side="left"
            )
        else:
            displayed = str(stored)
            if argument.name == "device" and self._device_labels:
                displayed = next(
                    (
                        label
                        for label, selector in self._device_labels.items()
                        if selector == displayed
                    ),
                    "",
                )
            variable = tk.StringVar(value=displayed)
            if argument.name == "device" and self._device_labels:
                control = ttk.Combobox(
                    control_row,
                    textvariable=variable,
                    values=tuple(self._device_labels),
                    state="readonly",
                )
                control.pack(side="left", fill="x", expand=True)
                control.bind(
                    "<<ComboboxSelected>>",
                    lambda _event: self._device_chosen(variable.get()),
                )
            elif argument.kind == "choice":
                ttk.Combobox(
                    control_row,
                    textvariable=variable,
                    values=argument.choices,
                    state="readonly",
                ).pack(side="left", fill="x", expand=True)
            else:
                if (
                    argument.name
                    in {
                        "project",
                        "sdk",
                        "root",
                        "store",
                        "workspace",
                        "output",
                    }
                    and displayed
                ):
                    ttk.Combobox(
                        control_row,
                        textvariable=variable,
                        values=(displayed,),
                    ).pack(side="left", fill="x", expand=True)
                else:
                    ttk.Entry(control_row, textvariable=variable).pack(
                        side="left", fill="x", expand=True
                    )
                if self._can_browse(argument):
                    ttk.Button(
                        control_row,
                        text="Browse…",
                        command=lambda: self._browse(argument, variable),
                    ).pack(side="left", padx=(7, 0))
        self._variables[argument.name] = variable
        help_text = argument.help
        if argument.default is not None and not stored:
            help_text += f"  Default: {argument.default}."
        if argument.repeatable:
            help_text += "  Separate multiple values with spaces."
        if help_text:
            help_label = ttk.Label(
                row,
                text=help_text.strip(),
                style="Muted.TLabel",
                wraplength=max(280, self.winfo_width() // 2),
                justify="left",
            )
            help_label.pack(anchor="w", pady=(3, 0))
            self._help_labels.append(help_label)
        error_label = ttk.Label(row, text="", foreground="#b42336")
        self._errors[argument.name] = error_label
        if argument.kind != "flag":
            variable.trace_add(
                "write",
                lambda *_args: self._validate_visible(argument, variable),
            )

    def _device_chosen(self, label: str) -> None:
        selector = self._device_labels.get(label)
        if selector and self._on_device_selected is not None:
            self._on_device_selected(selector)

    def _resize_detail(self, event: tk.Event) -> None:
        """Keep descriptions inside the available detail column."""
        width = max(280, event.width - 20)
        self.description.configure(wraplength=width)
        self.guidance.configure(wraplength=width)
        for label in self._help_labels:
            label.configure(wraplength=width)
        for label in self._review_labels:
            label.configure(wraplength=max(120, event.width - 210))

    def _form_mousewheel(self, event: tk.Event) -> None:
        """Move the whole action view from touchpad gestures over controls."""
        if not str(event.widget).startswith(str(self.form)):
            return
        direction = (
            -1
            if getattr(event, "num", 0) == 4 or getattr(event, "delta", 0) > 0
            else 1
        )
        if self.on_outer_wheel is not None:
            self.on_outer_wheel(direction)
        return "break"

    def _bind_form_wheel(self) -> None:
        """Bind newly created controls before native class wheel handlers."""

        def visit(widget: tk.Misc) -> None:
            for child in widget.winfo_children():
                if not getattr(child, "_form_wheel_bound", False):
                    for sequence in (
                        "<MouseWheel>",
                        "<Button-4>",
                        "<Button-5>",
                    ):
                        child.bind(sequence, self._form_mousewheel, add="+")
                    child._form_wheel_bound = True
                visit(child)

        visit(self.form)

    def _show_field_error(self, name: str, message: str) -> None:
        label = self._errors[name]
        label.configure(text=message)
        if message and not label.winfo_manager():
            label.pack(anchor="w", pady=(2, 0))
        elif not message and label.winfo_manager():
            label.pack_forget()

    def _can_browse(self, argument: CommandArgument) -> bool:
        if argument.repeatable:
            return False
        if argument.name in _DIRECTORY_FIELDS | _FILE_FIELDS:
            return True
        return argument.name in {"destination", "archive", "ticket"}

    def _browse(self, argument: CommandArgument, variable: tk.Variable) -> None:
        assert self._task is not None
        name = argument.name
        save_file = (name == "ticket" and self._task.path[-1] == "begin") or (
            name == "destination" and self._task.path == ("firmware", "export")
        )
        choose_directory = (
            name in _DIRECTORY_FIELDS
            or name == "archive"
            or (name == "destination" and not save_file)
        )
        if save_file:
            selected = filedialog.asksaveasfilename(parent=self)
        elif choose_directory:
            selected = filedialog.askdirectory(parent=self)
        else:
            selected = filedialog.askopenfilename(parent=self)
        if selected:
            variable.set(selected)

    def _validate_visible(
        self, argument: CommandArgument, variable: tk.Variable
    ) -> None:
        if self._task is not None:
            self._edited.add((self._task.path, argument.name))
        issue = validate_field(argument, variable.get())
        self._show_field_error(argument.name, issue.message if issue else "")

    def _render_review(self) -> None:
        assert self._task is not None
        values = self._drafts[self._task.path]
        for argument in self._task.arguments:
            value = values.get(argument.name, "")
            if not value:
                continue
            if argument.name == "device":
                value = next(
                    (
                        label
                        for label, selector in self._device_labels.items()
                        if selector == value
                    ),
                    str(value),
                )
            row = ttk.Frame(self.form)
            row.pack(fill="x", pady=(0, 6))
            ttk.Label(
                row,
                text=field_label(self._task.path, argument) + ":",
                width=25,
                style="Muted.TLabel",
            ).pack(side="left", anchor="nw")
            value_label = ttk.Label(
                row,
                text="Yes" if value is True else str(value),
                wraplength=max(120, self.winfo_width() // 2 - 210),
            )
            value_label.pack(side="left", anchor="nw")
            self._review_labels.append(value_label)
        if not any(values.values()):
            ttk.Label(
                self.form,
                text="The SDK defaults will be used.",
                style="Muted.TLabel",
            ).pack(anchor="w", pady=8)

    def _back(self) -> None:
        if self._step_index == 0:
            return
        self._save_current()
        if self._step_index == len(self._steps) and not self._include_optional:
            self._step_index = next(
                (
                    index
                    for index in range(len(self._steps) - 1, -1, -1)
                    if not self._steps[index].optional
                ),
                0,
            )
        else:
            self._step_index -= 1
        self.feedback.configure(text="")
        self._render_step()

    def _can_skip_remaining(self) -> bool:
        if self._task is None or self._include_optional:
            return False
        remaining = self._steps[self._step_index + 1 :]
        if not remaining or not all(step.optional for step in remaining):
            return False
        if self._task.path == ("init",):
            draft = self._drafts.get(self._task.path, {})
            return bool(
                draft.get("sdk")
                or (self._context and self._context.sdk_manifest)
            )
        return True

    def _configure_more(self) -> None:
        self._next_or_run(include_optional=True)

    def _next_or_run(self, include_optional: bool = False) -> None:
        if self._task is None:
            return
        self._save_current()
        draft = self._drafts[self._task.path]
        if self._step_index < len(self._steps):
            if (
                self._task.path == ("firmware", "import")
                and self._steps[self._step_index].title == "Source"
                and not any(draft.get(name) for _, name in _FIRMWARE_SOURCES)
            ):
                self.feedback.configure(text="Choose a firmware source.")
                return
            issues = [
                issue
                for argument in self._current_arguments
                if (
                    issue := validate_field(
                        argument, draft.get(argument.name, "")
                    )
                )
                is not None
            ]
            if issues:
                if any(issue.field not in self._errors for issue in issues):
                    self._advanced_open[(self._task.path, self._step_index)] = (
                        True
                    )
                    self._render_step()
                for issue in issues:
                    self._show_field_error(issue.field, issue.message)
                self.feedback.configure(text="Correct the highlighted fields.")
                return
            if (
                self._task.path == ("init",)
                and self._steps[self._step_index].title == "SDK and build"
                and not (
                    draft.get("sdk")
                    or (self._context and self._context.sdk_manifest)
                )
            ):
                self.feedback.configure(text="Select an installed SDK.")
                return
            if include_optional:
                self._include_optional = True
            self._step_index = (
                len(self._steps)
                if self._can_skip_remaining()
                else self._step_index + 1
            )
            self.feedback.configure(text="")
            self._render_step()
            return
        issues = validate_task(self._task, draft)
        if issues:
            self.feedback.configure(text=issues[0].message)
            for index, step in enumerate(self._steps):
                if any(
                    argument.name == issues[0].field
                    for argument in step.arguments
                ):
                    self._step_index = index
                    self._render_step()
                    self._show_field_error(issues[0].field, issues[0].message)
                    break
            return
        request = CommandRequest(
            path=self._task.path,
            argv=self._argument_tokens(self._task, draft),
        )
        self.feedback.configure(text="Working…")
        self.next_button.configure(state="disabled")
        self.outcome.hide()
        self._submit(
            lambda: self._client.run(request),
            self._completed,
            self._presentation(self._task).title,
        )

    @staticmethod
    def _argument_tokens(
        specification: CommandSpec, values: dict[str, str | bool]
    ) -> tuple[str, ...]:
        return argument_tokens(specification, values)

    def _completed(self, value: Any, error: Exception | None) -> None:
        self.next_button.configure(state="normal")
        if error is not None:
            self.feedback.configure(text="Task failed")
            status = Status.from_exception(error)
            self.outcome.show(
                OutcomeSummary(
                    title="Task failed",
                    message=f"{status.code.name}: {status.message}",
                ),
                status.model_dump(mode="json"),
            )
            return
        result: CommandResult = value
        self.feedback.configure(text="Completed")
        self.outcome.show(
            summarize_command(result),
            result.model_dump(mode="json", by_alias=True),
        )
