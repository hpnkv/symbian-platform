"""Host USB binding contracts that do not require a connected handset."""

import asyncio

import pytest

from symbian.device.usb import AsyncUsbSession, list_devices
from symbian.device.usb_models import UsbCompletion, UsbDeviceDescriptor
from symbian.native import require_native
from symbian.status import Code, StatusException


def test_native_inventory_redacts_serials() -> None:
    """The general USB inventory exposes location and descriptor IDs only."""
    devices = list_devices()
    assert isinstance(devices, list)
    assert all(isinstance(item, UsbDeviceDescriptor) for item in devices)
    assert all("serial" not in item.model_dump() for item in devices)
    assert all(
        {"vendor_id", "product_id", "bus", "address", "ports"}
        <= item.model_dump().keys()
        for item in devices
    )


def test_low_level_session_requires_serial_anchor() -> None:
    """The transport cannot open an arbitrary device by vendor and product."""
    native = require_native()
    with pytest.raises(StatusException):
        native.UsbSession.open(0x0421, 0x05D1, "bad")
    assert isinstance(native.list_usb_devices_native(), list)


@pytest.mark.parametrize(
    ("anchor", "path", "digest"),
    [
        ("bad", "Installs/app.sis", "0" * 64),
        ("0" * 24, "../app.sis", "0" * 64),
        ("0" * 24, "Installs/", "0" * 64),
        ("0" * 24, "Installs/app.sis", "bad"),
        ("0" * 24, "Installs/app.sis", "G" * 64),
    ],
)
def test_mtp_deletion_rejects_invalid_identity_path_or_digest(
    anchor, path, digest
):
    """Malformed deletion requests fail before opening any USB device."""
    with pytest.raises(StatusException) as caught:
        require_native().delete_mtp_file_native(
            0x0421, 0x05D1, anchor, 131073, path, digest
        )
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_async_adapter_returns_typed_future_result() -> None:
    """An asyncio pump settles a native-style transfer without busy waiting."""

    class FakeNativeSession:
        def __init__(self) -> None:
            self.future: asyncio.Future | None = None
            self.closed = False

        def control_in_future(self, *_arguments):
            self.future = asyncio.get_running_loop().create_future()
            return self.future

        def poll_fds(self):
            return []

        def next_timeout_ms(self):
            return 0

        def handle_events(self, _timeout_ms):
            if self.future is not None and not self.future.done():
                self.future.set_result(
                    UsbCompletion(
                        id=1, status="completed", actual_length=2, data=b"OK"
                    )
                )
            return []

        def close(self):
            self.closed = True

    async def run() -> None:
        native = FakeNativeSession()
        async with AsyncUsbSession(native) as session:
            completion = await session.control_in(0x80, 0, 0, 0, 2)
            assert isinstance(completion, UsbCompletion)
            assert completion.data == b"OK"
        assert native.closed

    asyncio.run(run())


def test_async_adapter_propagates_cancellation() -> None:
    """Cancelling the public Future also cancels its native Future."""

    class FakeNativeSession:
        future: asyncio.Future | None = None

        def interrupt_in_future(self, *_arguments):
            self.future = asyncio.get_running_loop().create_future()
            return self.future

        def poll_fds(self):
            return []

        def next_timeout_ms(self):
            return 1000

        def handle_events(self, _timeout_ms):
            return []

        def close(self):
            pass

    async def run() -> None:
        native = FakeNativeSession()
        async with AsyncUsbSession(native) as session:
            public_future = session.interrupt_in(0x83, 8)
            public_future.cancel()
            await asyncio.sleep(0)
            assert native.future is not None
            assert native.future.cancelled()

    asyncio.run(run())
