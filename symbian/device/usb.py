"""Low-level USB sessions backed by the host SDK's static libusb."""

import asyncio
import select

from symbian.device.connection import select as select_device
from symbian.device.usb_models import UsbCompletion, UsbDeviceDescriptor
from symbian.native import require_native
from symbian.status import Code, StatusException


def list_devices() -> list[UsbDeviceDescriptor]:
    """List USB descriptors and port paths without reading serial strings."""
    return [
        UsbDeviceDescriptor.model_validate(item)
        for item in require_native().list_usb_devices_native()
    ]


def open_selected(selector: str | None = None):
    """Open one serial-matched device for explicit USB transfer operations.

    Args:
        selector: Exact selector from ``symbian device list``, or ``None``
            when precisely one candidate is connected.

    Returns:
        A context-managed native ``UsbSession``. Call ``claim`` before using
        interface endpoints and ``handle_events`` to drain async completions.
    """
    device = select_device(selector)
    if device.identity_basis != "usb-serial":
        from symbian.status import Code, StatusError

        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Low-level USB access requires a serial-derived identity anchor",
        )
    return require_native().UsbSession.open(
        device.vendor_id, device.product_id, device.identity_anchor
    )


class AsyncUsbSession:
    """Drive native USB transfer Futures from the current asyncio loop.

    This adapter watches libusb file descriptors and checks its timeout while
    transfers are pending. The native libusb callback only queues completion;
    Python result conversion occurs after libusb returns from its event handler.
    """

    def __init__(self, native_session):
        self._loop = asyncio.get_running_loop()
        self._native = native_session
        self._pending: set[asyncio.Future] = set()
        self._readers: set[int] = set()
        self._writers: set[int] = set()
        self._timer = None
        self._closed = False
        self._native_closed = False
        self._legacy_completions: list[UsbCompletion] = []

    def _refresh(self) -> None:
        """Keep file-descriptor watches and one timeout aligned with libusb."""
        if self._timer is not None:
            self._timer.cancel()
            self._timer = None
        if self._closed or not self._pending:
            descriptors = []
        else:
            try:
                descriptors = self._native.poll_fds()
            except StatusException as error:
                if error.code != Code.UNIMPLEMENTED:
                    raise
                descriptors = []
        wanted_readers = {
            entry.fd for entry in descriptors if entry.events & select.POLLIN
        }
        wanted_writers = {
            entry.fd for entry in descriptors if entry.events & select.POLLOUT
        }
        for descriptor in self._readers - wanted_readers:
            self._loop.remove_reader(descriptor)
        for descriptor in self._writers - wanted_writers:
            self._loop.remove_writer(descriptor)
        for descriptor in wanted_readers - self._readers:
            self._loop.add_reader(descriptor, self._drive)
        for descriptor in wanted_writers - self._writers:
            self._loop.add_writer(descriptor, self._drive)
        self._readers = wanted_readers
        self._writers = wanted_writers
        if self._pending and not self._closed:
            next_timeout = self._native.next_timeout_ms()
            delay = (
                0.25
                if next_timeout < 0
                else min(0.25, max(0.001, next_timeout / 1000))
            )
            self._timer = self._loop.call_later(delay, self._drive)

    def _drive(self) -> None:
        """Pump ready libusb events without blocking the Python event loop."""
        if self._closed:
            return
        try:
            self._legacy_completions.extend(
                UsbCompletion.model_validate(item)
                for item in self._native.handle_events(0)
            )
            self._refresh()
        except Exception as error:
            self._closed = True
            self._refresh()
            for future in tuple(self._pending):
                if not future.done():
                    future.set_exception(error)

    def _submit(self, method, *arguments) -> asyncio.Future[UsbCompletion]:
        if self._closed:
            raise RuntimeError("USB session closed")
        native_future = method(*arguments)
        future = self._loop.create_future()
        self._pending.add(future)

        def copy_result(done: asyncio.Future) -> None:
            if future.done():
                return
            if done.cancelled():
                future.cancel()
            else:
                error = done.exception()
                if error is not None:
                    future.set_exception(error)
                else:
                    try:
                        future.set_result(
                            UsbCompletion.model_validate(done.result())
                        )
                    except Exception as conversion_error:
                        future.set_exception(conversion_error)

        def cancel_native(done: asyncio.Future) -> None:
            if done.cancelled():
                native_future.cancel()

        native_future.add_done_callback(copy_result)
        future.add_done_callback(cancel_native)

        def finished(done) -> None:
            self._pending.discard(done)
            self._refresh()

        future.add_done_callback(finished)
        self._refresh()
        self._loop.call_soon(self._drive)
        return future

    def bulk_in(
        self, endpoint: int, length: int, timeout_ms: int = 1500
    ) -> asyncio.Future[UsbCompletion]:
        """Return an asyncio Future for one bulk IN transfer."""
        return self._submit(
            self._native.bulk_in_future, endpoint, length, timeout_ms
        )

    def bulk_out(
        self, endpoint: int, data: bytes, timeout_ms: int = 1500
    ) -> asyncio.Future[UsbCompletion]:
        """Return an asyncio Future for one bulk OUT transfer."""
        return self._submit(
            self._native.bulk_out_future, endpoint, data, timeout_ms
        )

    def interrupt_in(
        self, endpoint: int, length: int, timeout_ms: int = 1500
    ) -> asyncio.Future[UsbCompletion]:
        """Return an asyncio Future for one interrupt IN transfer."""
        return self._submit(
            self._native.interrupt_in_future, endpoint, length, timeout_ms
        )

    def interrupt_out(
        self, endpoint: int, data: bytes, timeout_ms: int = 1500
    ) -> asyncio.Future[UsbCompletion]:
        """Return an asyncio Future for one interrupt OUT transfer."""
        return self._submit(
            self._native.interrupt_out_future, endpoint, data, timeout_ms
        )

    def control_in(
        self,
        request_type: int,
        request: int,
        value: int,
        index: int,
        length: int,
        timeout_ms: int = 1500,
    ) -> asyncio.Future[UsbCompletion]:
        """Return an asyncio Future for one control IN transfer."""
        return self._submit(
            self._native.control_in_future,
            request_type,
            request,
            value,
            index,
            length,
            timeout_ms,
        )

    def control_out(
        self,
        request_type: int,
        request: int,
        value: int,
        index: int,
        data: bytes,
        timeout_ms: int = 1500,
    ) -> asyncio.Future[UsbCompletion]:
        """Return an asyncio Future for one control OUT transfer."""
        return self._submit(
            self._native.control_out_future,
            request_type,
            request,
            value,
            index,
            data,
            timeout_ms,
        )

    def take_legacy_completions(self) -> list[UsbCompletion]:
        """Return transfer-ID completions collected while driving Futures."""
        completed, self._legacy_completions = self._legacy_completions, []
        return completed

    async def close(self) -> None:
        """Cancel pending Futures, then close the native USB session."""
        if self._native_closed:
            return
        for future in tuple(self._pending):
            future.cancel()
        for _ in range(40):
            if not self._pending:
                break
            self._drive()
            await asyncio.sleep(0.05)
        self._closed = True
        self._refresh()
        self._native.close()
        self._native_closed = True

    async def __aenter__(self):
        return self

    async def __aexit__(self, _error_type, _error, _traceback):
        await self.close()
