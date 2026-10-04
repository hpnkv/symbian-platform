"""Frontend-independent HTTPX client for the in-process console API."""

import httpx
from fastapi import FastAPI

from symbian.console.models import (
    CommandCatalog,
    CommandRequest,
    CommandResult,
    ConsoleContext,
    DeviceInspectRequest,
)
from symbian.console.service import create_app
from symbian.device.connection import DeviceInfoResult, DeviceListResult
from symbian.device.usb_models import UsbDeviceDescriptor
from symbian.status import Status


class ConsoleClient:
    """Call the SDK service through HTTPX without opening a network port."""

    def __init__(self, application: FastAPI | None = None) -> None:
        self._application = application or create_app()

    async def _request(
        self,
        method: str,
        path: str,
        body: dict | None = None,
        params: dict[str, str] | None = None,
    ) -> object:
        transport = httpx.ASGITransport(app=self._application)
        async with httpx.AsyncClient(
            transport=transport, base_url="http://symbian-console.local"
        ) as connection:
            try:
                response = await connection.request(
                    method, path, json=body, params=params
                )
                if response.is_error:
                    try:
                        status = Status.model_validate(response.json())
                    except (ValueError, TypeError):
                        response.raise_for_status()
                    else:
                        raise status.to_exception()
                return response.json()
            except httpx.HTTPError as error:
                raise Status.from_exception(error).to_exception() from error

    async def catalog(self) -> CommandCatalog:
        """List all current CLI workflows and their generated form fields."""
        return CommandCatalog.model_validate(
            await self._request("GET", "/api/v1/catalog")
        )

    async def context(
        self,
        workspace: str | None = None,
        project: str | None = None,
        sdk: str | None = None,
    ) -> ConsoleContext:
        """Resolve actual current directories, SDK and connected phones."""
        overrides = {
            key: value
            for key, value in (
                ("workspace", workspace),
                ("project", project),
                ("sdk", sdk),
            )
            if value is not None
        }
        return ConsoleContext.model_validate(
            await self._request("GET", "/api/v1/context", params=overrides)
        )

    async def run(self, request: CommandRequest) -> CommandResult:
        """Run one catalogued SDK command."""
        return CommandResult.model_validate(
            await self._request(
                "POST", "/api/v1/commands", request.model_dump(mode="json")
            )
        )

    async def devices(self) -> DeviceListResult:
        """List supported Symbian handsets."""
        return DeviceListResult.model_validate(
            await self._request("GET", "/api/v1/devices")
        )

    async def inspect(self, request: DeviceInspectRequest) -> DeviceInfoResult:
        """Run a bounded USB or protocol probe."""
        return DeviceInfoResult.model_validate(
            await self._request(
                "POST",
                "/api/v1/devices/inspect",
                request.model_dump(mode="json"),
            )
        )

    async def usb_inventory(self) -> tuple[UsbDeviceDescriptor, ...]:
        """Inspect generic USB descriptors without serial strings."""
        response = await self._request("GET", "/api/v1/usb/inventory")
        return tuple(
            UsbDeviceDescriptor.model_validate(item) for item in response
        )
