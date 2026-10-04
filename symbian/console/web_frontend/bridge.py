"""Thread-safe pywebview bridge to the typed in-memory console client."""

import asyncio
import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path
from tempfile import TemporaryDirectory
from threading import RLock
from typing import Any

import webview

from symbian.console.application import read_application
from symbian.console.arguments import argument_tokens
from symbian.console.client import ConsoleClient
from symbian.console.defaults import initial_value
from symbian.console.device_presentation import (
    summarize_device,
    usb_class_name,
)
from symbian.console.device_status import DeviceStatusTracker
from symbian.console.models import (
    CommandCatalog,
    CommandRequest,
    ConsoleContext,
    DeviceInspectRequest,
)
from symbian.console.presentation import (
    PRESENTATIONS,
    summarize_command,
    task_steps,
)
from symbian.console.selection import DeviceSelection
from symbian.console.syntax import highlighted_spans
from symbian.console.validation import field_label, validate_task
from symbian.console.web_frontend.models import (
    CommandOutcome,
    ConsoleSnapshot,
    DesktopCatalog,
    DesktopTask,
    DeviceOutcome,
    FirmwareLibrary,
    FormDefault,
    FormDefaults,
    FormValues,
    HostSelectionRequest,
    SyntaxSpan,
    UsbInventoryItem,
    UsbTopology,
)
from symbian.device.agent_observation import (
    clear_report,
    read_for_devices,
    report_running,
)
from symbian.device.connection import ConnectedDevice
from symbian.project.layout import has_application_manifest
from symbian.status import Code, Status, StatusException


class ConsoleWebBridge:
    """Expose bounded SDK actions to one local, serverless webview."""

    def __init__(
        self,
        client: ConsoleClient | None = None,
        workdir: Path | None = None,
        initial_application: bool = False,
    ) -> None:
        self._client = client or ConsoleClient()
        self._worker = ThreadPoolExecutor(
            max_workers=2, thread_name_prefix="console-web"
        )
        self._lock = RLock()
        self._selection = DeviceSelection()
        self._status = DeviceStatusTracker()
        self._devices: tuple[ConnectedDevice, ...] = ()
        self._context: ConsoleContext | None = None
        self._catalog: CommandCatalog | None = None
        self._workspace_override: str | None = (
            str(workdir.expanduser().resolve()) if workdir else None
        )
        self._project_override: str | None = None
        self._sdk_override: str | None = None
        self._window: webview.Window | None = None
        self._initial_application = False
        if initial_application and workdir is not None:
            try:
                read_application(workdir)
                self._initial_application = True
            except StatusException:
                pass
        self._private_files = TemporaryDirectory(prefix="symbian-console-")
        self._build_log = Path(self._private_files.name) / "build.log"
        self._run_session = Path(self._private_files.name) / "run-session.txt"
        self._run_project: Path | None = None

    def attach_window(self, window: webview.Window) -> None:
        """Attach the native shell for file and folder dialogs."""
        self._window = window

    def get_startup_view(self) -> str:
        """Choose an application view only for an explicit valid workdir."""
        return (
            "application_detail"
            if self._initial_application
            else "applications"
        )

    def get_agent_project(self) -> dict[str, Any]:
        """Locate the SDK checkout's development agent project.

        A packaged SDK may omit this project. The frontend must not invent a
        deployable phone agent when its source is unavailable.
        """
        project = Path(__file__).resolve().parents[3] / "agent_service"
        with self._lock:
            manifest = self._context.sdk_manifest if self._context else None
        compiler = linker = None
        if manifest:
            try:
                sdk = json.loads(Path(manifest).read_text(encoding="utf-8"))
                compiler, linker = sdk.get("compiler"), sdk.get("linker")
            except (OSError, ValueError):
                pass
        return {
            "project": (
                str(project) if (project / "symbian.toml").is_file() else None
            ),
            "profile": "emulator",
            "compiler": compiler,
            "linker": linker,
        }

    def report_agent_running(self, selector: str) -> dict[str, Any]:
        """Record a selected phone owner's observation, without USB claims."""
        with self._lock:
            device = next(
                (item for item in self._devices if item.selector == selector),
                None,
            )
        if device is None:
            raise Status(
                code=Code.NOT_FOUND, message="Selected phone is unavailable"
            ).to_exception()
        return report_running(device).model_dump(mode="json")

    def clear_agent_report(self, selector: str) -> None:
        """Forget a stale owner report for the selected phone."""
        with self._lock:
            device = next(
                (item for item in self._devices if item.selector == selector),
                None,
            )
        if device is None:
            raise Status(
                code=Code.NOT_FOUND, message="Selected phone is unavailable"
            ).to_exception()
        clear_report(device)

    def _call(self, coroutine: Any) -> Any:
        return self._worker.submit(lambda: asyncio.run(coroutine)).result()

    def get_catalog(self) -> dict[str, Any]:
        """Return the generated public CLI action catalog."""
        catalog = self._call(self._client.catalog())
        with self._lock:
            self._catalog = catalog
        result = DesktopCatalog(
            tasks=tuple(
                DesktopTask(
                    command=command.model_copy(
                        update={
                            "arguments": tuple(
                                argument.model_copy(
                                    update={
                                        "label": field_label(
                                            command.path, argument
                                        )
                                    }
                                )
                                for argument in command.arguments
                            )
                        }
                    ),
                    presentation=PRESENTATIONS[command.path],
                    steps=tuple(
                        step.model_copy(
                            update={
                                "arguments": tuple(
                                    argument.model_copy(
                                        update={
                                            "label": field_label(
                                                command.path, argument
                                            )
                                        }
                                    )
                                    for argument in step.arguments
                                )
                            }
                        )
                        for step in task_steps(command)
                    ),
                )
                for command in catalog.commands
                if command.path in PRESENTATIONS
            )
        )
        return result.model_dump(mode="json")

    def get_context(self) -> dict[str, Any]:
        """Refresh host context and reconcile the selected phone."""
        with self._lock:
            workspace = self._workspace_override
            project = self._project_override
            sdk = self._sdk_override
        try:
            context = self._call(self._client.context(workspace, project, sdk))
        except (StatusException, OSError, ValueError):
            with self._lock:
                self._status.lose_observation()
            raise
        with self._lock:
            self._context = context
            self._devices = context.devices
            selected = self._selection.refresh(context.devices)
            self._status.observe(context.devices, selected)
            snapshot = ConsoleSnapshot(
                context=context,
                selected_device=selected,
                device_status=self._status.view(),
            )
        result = snapshot.model_dump(mode="json")
        result["agent_observations"] = read_for_devices(context.devices)
        return result

    def get_application_overview(self) -> dict[str, Any]:
        """Read the selected application's identity and available artifact."""
        with self._lock:
            context = self._context
            project = context.project if context else None
        if project is None:
            raise Status(
                code=Code.FAILED_PRECONDITION,
                message="Select an application first",
            ).to_exception()
        firmware = None
        try:
            from symbian.emulator.configuration import resolve

            settings = resolve(
                project=Path(project),
                sdk=(
                    Path(context.sdk_manifest) if context.sdk_manifest else None
                ),
                root=Path(context.workspace),
            ).settings
            firmware = settings.firmware
        except Exception:
            # The declared project remains inspectable without emulator setup.
            pass
        return read_application(Path(project), firmware).model_dump(mode="json")

    def select_host_path(self, submitted: dict[str, Any]) -> dict[str, Any]:
        """Select a real workspace, application, or installed SDK."""
        request = HostSelectionRequest.model_validate(submitted)
        chosen = request.path.strip()
        if request.kind == "workspace" and not chosen:
            raise Status(
                code=Code.INVALID_ARGUMENT,
                message="Choose a working directory",
            ).to_exception()
        if chosen:
            path = Path(chosen).expanduser().resolve()
            candidate = (
                path / "sdk.json"
                if request.kind == "sdk" and path.is_dir()
                else path
            )
            valid = (
                path.is_dir()
                if request.kind == "workspace"
                else (
                    has_application_manifest(path)
                    if request.kind == "project"
                    else candidate.is_file() and candidate.name == "sdk.json"
                )
            )
            if not valid:
                labels = {
                    "workspace": "Choose an existing directory",
                    "project": (
                        "Choose a folder containing symbian.toml or "
                        "symbian-project.json"
                    ),
                    "sdk": "Choose an SDK folder containing sdk.json",
                }
                raise Status(
                    code=Code.INVALID_ARGUMENT,
                    message=labels[request.kind],
                ).to_exception()
            chosen = str(candidate if request.kind == "sdk" else path)
        with self._lock:
            previous = (
                self._workspace_override,
                self._project_override,
                self._sdk_override,
            )
            if request.kind == "workspace":
                self._workspace_override = chosen
                self._project_override = None
                self._sdk_override = None
            elif request.kind == "project":
                self._project_override = chosen
                self._sdk_override = None
            else:
                self._sdk_override = chosen
        try:
            return self.get_context()
        except Exception:
            with self._lock:
                (
                    self._workspace_override,
                    self._project_override,
                    self._sdk_override,
                ) = previous
            raise

    def get_usb_topology(self) -> dict[str, Any]:
        """Read a cheap native USB observation outside the SDK work queue."""
        from symbian.device.usb import list_devices

        return UsbTopology(devices=tuple(list_devices())).model_dump(
            mode="json"
        )

    def get_form_defaults(self, path: list[str]) -> dict[str, Any]:
        """Resolve visible values from the latest host and phone context."""
        key = tuple(path)
        with self._lock:
            specification = self._specification(key)
            context = self._context
            selected = self._selection.current
        defaults = FormDefaults(
            path=key,
            values=tuple(
                FormDefault(
                    name=argument.name,
                    value=initial_value(
                        context,
                        key,
                        argument,
                        selected.selector if selected else None,
                    ),
                )
                for argument in specification.arguments
            ),
        )
        return defaults.model_dump(mode="json")

    def validate_form(self, submitted: dict[str, Any]) -> list[dict[str, Any]]:
        """Validate guided inputs using the shared Python policy."""
        form = FormValues.model_validate(submitted)
        with self._lock:
            specification = self._specification(form.path)
        return [
            issue.model_dump(mode="json")
            for issue in validate_task(specification, form.values)
        ]

    def _specification(self, path: tuple[str, ...]):
        catalog = self._catalog
        if catalog is None:
            raise Status(
                code=Code.FAILED_PRECONDITION,
                message="The action catalog is not loaded.",
            ).to_exception()
        for command in catalog.commands:
            if command.path == path:
                return command
        raise Status(
            code=Code.NOT_FOUND,
            message="The selected SDK action is unavailable.",
        ).to_exception()

    def get_usb_inventory(self) -> list[dict[str, Any]]:
        """Return serial-redacted native libusb descriptors."""
        devices = self._call(self._client.usb_inventory())
        return [
            UsbInventoryItem.model_validate(
                {
                    **device.model_dump(mode="json"),
                    "class_name": usb_class_name(device.device_class),
                }
            ).model_dump(mode="json")
            for device in devices
        ]

    def select_device(self, selector: str) -> dict[str, Any] | None:
        """Apply a live selection and return the new status indication."""
        with self._lock:
            if selector:
                self._selection.select(selector, self._devices)
            else:
                self._selection.clear()
            self._status.observe(
                self._devices,
                (
                    self._selection.current.selector
                    if self._selection.current
                    else None
                ),
            )
            view = self._status.view()
        return view.model_dump(mode="json") if view else None

    def get_device_status(self) -> dict[str, Any] | None:
        """Read pending work without waiting for a device request."""
        with self._lock:
            view = self._status.view()
        return view.model_dump(mode="json") if view else None

    def clear_pending_device_status(self) -> dict[str, Any] | None:
        """Dismiss a local mode hint without modifying its saved ticket."""
        with self._lock:
            self._status.clear_pending()
            view = self._status.view()
        return view.model_dump(mode="json") if view else None

    def run_form(self, submitted: dict[str, Any]) -> dict[str, Any]:
        """Validate and execute one guided action without JS flag logic."""
        form = FormValues.model_validate(submitted)
        with self._lock:
            specification = self._specification(form.path)
        issues = validate_task(specification, form.values)
        if issues:
            raise Status(
                code=Code.INVALID_ARGUMENT,
                message=issues[0].message,
            ).to_exception()
        building = form.path in (("build",), ("app", "build"))
        running = form.path == ("app", "run")
        if building or running:
            self._build_log.write_text("", encoding="utf-8")
        if running:
            self._run_session.unlink(missing_ok=True)
            selected_project = form.values.get("project")
            self._run_project = (
                Path(selected_project).expanduser().resolve()
                if isinstance(selected_project, str) and selected_project
                else None
            )
        command = CommandRequest(
            path=form.path,
            argv=argument_tokens(specification, form.values),
            cwd=self._context.workspace if self._context else None,
            sdk_manifest=(
                self._context.sdk_manifest if self._context else None
            ),
            build_log_path=(
                str(self._build_log) if building or running else None
            ),
            run_session_path=str(self._run_session) if running else None,
        )
        token = None
        with self._lock:
            if command.path and command.path[0] == "device":
                presentation = PRESENTATIONS.get(command.path)
                label = presentation.title if presentation else "Device action"
                token = self._status.begin(label, command.path)
        success = False
        try:
            result = self._call(self._client.run(command))
            success = True
            return CommandOutcome(
                result=result,
                summary=summarize_command(result),
                firmware_library=(
                    FirmwareLibrary.model_validate(result.result)
                    if command.path == ("firmware", "list")
                    else None
                ),
            ).model_dump(mode="json")
        finally:
            if token is not None:
                with self._lock:
                    self._status.finish(token, success)

    def get_build_log(self) -> str:
        """Read a bounded tail of the active private build log."""
        return self._read_log_tail(self._build_log, 65536)

    @staticmethod
    def _read_log_tail(path: Path, limit: int) -> str:
        """Read recent UTF-8 output without loading an unbounded log."""
        try:
            with path.open("rb") as log:
                log.seek(0, 2)
                log.seek(max(0, log.tell() - limit))
                return log.read().decode("utf-8", errors="replace")
        except FileNotFoundError:
            return ""

    def get_run_log(self) -> str:
        """Read live tool output and the selected emulator frontend log."""
        tool_output = self._read_log_tail(self._build_log, 16384)
        project = self._run_project
        if project is None:
            return tool_output
        try:
            session_name = self._run_session.read_text(encoding="utf-8")
        except FileNotFoundError:
            session_name = next(
                (
                    line.removeprefix("Emulator session: ")
                    for line in reversed(tool_output.splitlines())
                    if line.startswith("Emulator session: ")
                ),
                "",
            )
        if not session_name:
            return tool_output
        session_directory = Path(session_name).resolve()
        if not session_directory.is_relative_to(project / ".symbian/runs"):
            return tool_output
        frontend = self._read_log_tail(
            session_directory / "frontend.log", 49152
        )
        return tool_output.rstrip() + (
            "\n\nEmulator output\n" + frontend if frontend else ""
        )

    def inspect_device(self, request: dict[str, Any]) -> dict[str, Any]:
        """Run one bounded native USB, AT, MTP or OBEX probe."""
        inspection = DeviceInspectRequest.model_validate(request)
        with self._lock:
            if inspection.selector is not None:
                self._selection.select(inspection.selector, self._devices)
                self._status.observe(
                    self._devices,
                    (
                        self._selection.current.selector
                        if self._selection.current
                        else None
                    ),
                )
            token = self._status.begin(
                f"Read {inspection.operation}",
                ("device", "protocol", inspection.operation),
            )
        success = False
        try:
            result = self._call(self._client.inspect(inspection))
            success = True
            return DeviceOutcome(
                result=result,
                summary=summarize_device(inspection.operation, result),
            ).model_dump(mode="json")
        finally:
            with self._lock:
                self._status.finish(token, success)

    def choose_path(self, kind: str, current: str = "") -> str | None:
        """Use the host's native file or folder picker for one form input."""
        window = self._window
        if window is None:
            return None
        dialog = {
            "folder": webview.FOLDER_DIALOG,
            "save": webview.SAVE_DIALOG,
        }.get(kind, webview.OPEN_DIALOG)
        directory = Path(current).expanduser()
        if directory.is_file():
            directory = directory.parent
        selected = window.create_file_dialog(
            dialog, directory=str(directory) if kind == "folder" else ""
        )
        return str(selected[0]) if selected else None

    def highlight(self, content: str, language: str) -> list[dict[str, Any]]:
        """Return bounded Pygments fragments for a technical detail view."""
        return [
            SyntaxSpan(category=category, text=fragment).model_dump(mode="json")
            for category, fragment in highlighted_spans(
                content[:262144], language
            )
        ]

    def shutdown(self) -> None:
        """Stop the SDK worker after the desktop window closes."""
        self._worker.shutdown(wait=False, cancel_futures=True)
        self._private_files.cleanup()
