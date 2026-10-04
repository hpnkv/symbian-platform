"""Presence and pending-operation behavior for the desktop status bar."""

from symbian.console.device_status import DeviceStatusTracker
from symbian.device.connection import ConnectedDevice


def _phone() -> ConnectedDevice:
    """Construct one serial-anchored handset observation."""
    return ConnectedDevice(
        selector="usb:phone",
        manufacturer="Nokia",
        product="808 PureView",
        vendor_id=0x0421,
        product_id=0x05D1,
        location_id=0x14,
        identity_anchor="redacted-anchor",
        identity_basis="usb-serial",
        interface_profile="composite",
    )


def test_idle_disconnect_clears_device_status() -> None:
    tracker = DeviceStatusTracker()
    phone = _phone()
    tracker.observe((phone,), phone.selector)
    view = tracker.view()
    assert view is not None
    assert view.tone == "connected"
    assert "808 PureView" in view.text
    assert "0421:05d1" in view.text

    tracker.observe((), None)
    assert tracker.view() is None


def test_active_operation_survives_disconnect_until_result() -> None:
    tracker = DeviceStatusTracker()
    phone = _phone()
    tracker.observe((phone,), phone.selector)
    token = tracker.begin(
        "Read identity", ("device", "protocol", "at-identity")
    )
    tracker.observe((), None)
    view = tracker.view()
    assert view is not None
    assert view.tone == "pending"
    assert "disconnected; awaiting result" in view.text

    tracker.finish(token, success=False)
    view = tracker.view()
    assert view is not None
    assert "connection unverified" in view.text
    tracker.observe((), None)
    assert tracker.view() is None


def test_mode_change_remains_pending_until_verified_or_cleared() -> None:
    tracker = DeviceStatusTracker()
    phone = _phone()
    tracker.observe((phone,), phone.selector)
    begin = tracker.begin("Save baseline", ("device", "mode", "begin"))
    tracker.finish(begin, success=True)
    tracker.observe((), None)
    view = tracker.view()
    assert view is not None
    assert view.tone == "pending"
    assert "verification pending" in view.text

    verify = tracker.begin("Verify mode change", ("device", "mode", "verify"))
    tracker.finish(verify, success=True)
    assert tracker.view() is None

    tracker.observe((phone,), phone.selector)
    begin = tracker.begin("Save baseline", ("device", "mode", "begin"))
    tracker.finish(begin, success=True)
    tracker.clear_pending()
    assert tracker.view().tone == "connected"
