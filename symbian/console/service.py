"""SDK operations behind a local-only, in-process console API."""

import argparse
import time
from collections.abc import Callable
from pathlib import Path
from typing import Any

from fastapi import FastAPI
from fastapi.encoders import jsonable_encoder
from fastapi.exceptions import RequestValidationError
from fastapi.responses import JSONResponse

from symbian.console.catalog import catalog
from symbian.console.models import (
    CommandCatalog,
    CommandRequest,
    CommandResult,
    ConsoleContext,
    DeviceInspectRequest,
)
from symbian.device.connection import DeviceInfoResult, DeviceListResult
from symbian.device.usb_models import UsbDeviceDescriptor
from symbian.status import Status, StatusCode, StatusException


class _ArgumentFailure(Exception):
    """Argparse rejection without process exit or stderr output."""


def _raise_argument_error(message: str) -> None:
    raise _ArgumentFailure(message)


def _disable_argparse_exit(parser: argparse.ArgumentParser) -> None:
    """Report parser errors through the API instead of exiting."""
    parser.error = _raise_argument_error
    parser.exit = lambda _status=0, message=None: _raise_argument_error(
        message or "Help is available in the command form"
    )
    for action in parser._actions:
        if isinstance(action, argparse._SubParsersAction):
            for child in action.choices.values():
                _disable_argparse_exit(child)


class ConsoleService:
    """Dispatch catalogued SDK workflows and bounded device probes."""

    def __init__(
        self,
        command_runner: Callable[[argparse.Namespace], Any] | None = None,
    ) -> None:
        self.catalog = catalog()
        self._commands = {spec.path: spec for spec in self.catalog.commands}
        self._command_runner = command_runner
        self._cached_devices: DeviceListResult | None = None
        self._cached_usb_signature: (
            tuple[tuple[int, int, int, int, tuple[int, ...]], ...] | None
        ) = None
        self._cached_devices_at = 0.0

    def run_command(self, request: CommandRequest) -> CommandResult:
        """Execute a catalogued command using the canonical argparse policy."""
        if request.path not in self._commands:
            raise Status(
                code=StatusCode.NOT_FOUND,
                message="Unknown console workflow",
            ).to_exception()
        if request.path[0] == "device" and request.path[1] not in {
            "list",
            "info",
            "mode",
            "install",
            "policy",
        }:
            raise Status(
                code=StatusCode.PERMISSION_DENIED,
                message="Device operation has no console executor",
            ).to_exception()
        if len(request.argv) > 256 or any(
            len(value) > 4096 for value in request.argv
        ):
            raise Status(
                code=StatusCode.INVALID_ARGUMENT,
                message="Command input too large",
            ).to_exception()
        if any(value in {"-h", "--help"} for value in request.argv):
            raise Status(
                code=StatusCode.INVALID_ARGUMENT,
                message="Use the command form for argument guidance",
            ).to_exception()
        if request.cwd is not None and not Path(request.cwd).is_dir():
            raise Status(
                code=StatusCode.INVALID_ARGUMENT,
                message="Selected working directory does not exist",
            ).to_exception()
        from symbian.cli.__main__ import _parser

        parser = _parser()
        _disable_argparse_exit(parser)
        try:
            arguments = parser.parse_args([*request.path, *request.argv])
            if self._command_runner is not None:
                result = self._command_runner(arguments)
                return CommandResult(path=request.path, result=result)
            from symbian.console.runner import run_cli

            return run_cli(request)
        except _ArgumentFailure as error:
            raise Status(
                code=StatusCode.INVALID_ARGUMENT, message=str(error)
            ).to_exception() from error
        except OSError as error:
            raise Status(
                code=StatusCode.UNAVAILABLE, message=str(error)
            ).to_exception() from error

    def list_devices(self) -> DeviceListResult:
        """List SDK-supported devices with serial-redacted identities."""
        from symbian.device.connection import list_devices

        return DeviceListResult.model_validate(list_devices())

    def current_context(
        self,
        workspace: str | None = None,
        project: str | None = None,
        sdk: str | None = None,
    ) -> ConsoleContext:
        """Resolve current host paths and discover supported phones."""
        from symbian.device.connection import list_devices
        from symbian.emulator.configuration import resolve
        from symbian.project.configuration import ProjectConfiguration
        from symbian.project.layout import has_application_manifest
        from symbian.project.sdk import discover_sdk

        workspace_path = (
            Path(workspace).expanduser().resolve()
            if workspace
            else Path.cwd().resolve()
        )
        if not workspace_path.is_dir():
            raise Status(
                code=StatusCode.INVALID_ARGUMENT,
                message="Selected working directory does not exist",
            ).to_exception()
        project_path = (
            (
                workspace_path
                if has_application_manifest(workspace_path)
                else None
            )
            if project is None
            else Path(project).expanduser().resolve() if project else None
        )
        if project_path is not None and not has_application_manifest(
            project_path
        ):
            raise Status(
                code=StatusCode.INVALID_ARGUMENT,
                message=(
                    "Selected application has no symbian.toml or "
                    "symbian-project.json"
                ),
            ).to_exception()
        sdk_manifest = None
        if sdk is not None:
            if sdk:
                candidate = discover_sdk(Path(sdk).expanduser())
                if not candidate.is_file():
                    raise Status(
                        code=StatusCode.INVALID_ARGUMENT,
                        message="Selected SDK has no sdk.json",
                    ).to_exception()
                sdk_manifest = candidate
        elif project_path is not None and (
            (project_path / "symbian-project.json").is_file()
            or (project_path / "sdk-location.json").is_file()
        ):
            try:
                if (project_path / "symbian-project.json").is_file():
                    project_sdk = ProjectConfiguration.load(
                        project_path
                    ).sdk_location
                else:
                    import json

                    declared = Path(
                        json.loads(
                            (project_path / "sdk-location.json").read_text()
                        )["sdk"]
                    )
                    project_sdk = (
                        declared
                        if declared.is_absolute()
                        else (project_path / declared).resolve()
                    )
                candidate = project_sdk / "sdk.json"
                sdk_manifest = candidate if candidate.is_file() else None
            except (StatusException, OSError, ValueError, KeyError, TypeError):
                pass
        if sdk_manifest is None and sdk is None:
            try:
                local_sdk = workspace_path / ".symbian/app-sdk/sdk.json"
                selected_sdk = (
                    local_sdk if local_sdk.is_file() else discover_sdk()
                )
                sdk_manifest = selected_sdk if selected_sdk.is_file() else None
            except StatusException:
                pass
        try:
            settings = resolve(
                project=project_path, sdk=sdk_manifest, root=workspace_path
            ).settings
            firmware_store = (
                str(settings.store) if settings.store is not None else None
            )
        except StatusException:
            firmware_store = None
        signature = None
        try:
            signature = tuple(
                sorted(
                    (
                        item.vendor_id,
                        item.product_id,
                        item.bus,
                        item.address,
                        item.ports,
                    )
                    for item in self.usb_inventory()
                )
            )
        except (StatusException, OSError):
            # Discovery still works on hosts where the native USB extension
            # cannot provide the cheap change signal.
            pass
        now = time.monotonic()
        if (
            signature is not None
            and signature == self._cached_usb_signature
            and self._cached_devices is not None
            and now - self._cached_devices_at < 20
        ):
            inventory = self._cached_devices
        else:
            inventory = DeviceListResult.model_validate(list_devices())
            self._cached_devices = inventory
            self._cached_usb_signature = signature
            self._cached_devices_at = now
        return ConsoleContext(
            workspace=str(workspace_path),
            project=str(project_path) if project_path else None,
            sdk_manifest=str(sdk_manifest) if sdk_manifest else None,
            firmware_store=firmware_store,
            devices=inventory.devices,
        )

    def inspect_device(self, request: DeviceInspectRequest) -> DeviceInfoResult:
        """Inspect USB, AT, MTP, or OBEX through existing bounded SDK flows."""
        from symbian.device.connection import inspect_device

        operation = request.operation
        return DeviceInfoResult.model_validate(
            inspect_device(
                request.selector,
                probe_protocol=operation in {"at-identity", "at-status"},
                at_status=operation == "at-status",
                mtp=operation in {"mtp", "mtp-list"},
                mtp_list=request.limit if operation == "mtp-list" else 0,
                obex_connect=operation == "obex",
            )
        )

    def usb_inventory(self) -> list[UsbDeviceDescriptor]:
        """Return generic libusb descriptors without serial strings."""
        from symbian.device.usb import list_devices

        return list_devices()


def create_app(service: ConsoleService | None = None) -> FastAPI:
    """Build an ASGI app used only by an in-memory HTTPX transport."""
    operations = service or ConsoleService()
    application = FastAPI(
        title="Symbian Console API",
        version="1",
        docs_url=None,
        redoc_url=None,
        openapi_url=None,
    )

    @application.exception_handler(StatusException)
    async def status_failure(
        _request: Any, error: StatusException
    ) -> JSONResponse:
        return JSONResponse(
            status_code=error.code.to_http_code(),
            content=jsonable_encoder(error.status),
        )

    @application.exception_handler(RequestValidationError)
    async def invalid_request(
        _request: Any, error: RequestValidationError
    ) -> JSONResponse:
        status = Status(
            code=StatusCode.INVALID_ARGUMENT,
            message=str(error),
        )
        return JSONResponse(
            status_code=status.code.to_http_code(),
            content=jsonable_encoder(status),
        )

    @application.get("/api/v1/catalog", response_model=CommandCatalog)
    async def get_catalog() -> CommandCatalog:
        return operations.catalog

    @application.get("/api/v1/context", response_model=ConsoleContext)
    async def get_context(
        workspace: str | None = None,
        project: str | None = None,
        sdk: str | None = None,
    ) -> ConsoleContext:
        return operations.current_context(workspace, project, sdk)

    @application.post("/api/v1/commands", response_model=CommandResult)
    async def run_command(request: CommandRequest) -> CommandResult:
        return operations.run_command(request)

    @application.get("/api/v1/devices", response_model=DeviceListResult)
    async def get_devices() -> DeviceListResult:
        return operations.list_devices()

    @application.post(
        "/api/v1/devices/inspect", response_model=DeviceInfoResult
    )
    async def inspect_device(request: DeviceInspectRequest) -> DeviceInfoResult:
        return operations.inspect_device(request)

    @application.get(
        "/api/v1/usb/inventory", response_model=list[UsbDeviceDescriptor]
    )
    async def get_usb_inventory() -> list[UsbDeviceDescriptor]:
        return operations.usb_inventory()

    return application
