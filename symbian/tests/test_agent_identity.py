"""Phone-bound agent key storage and pairing boundaries."""

import hashlib
import json
import stat
from pathlib import Path

import pytest

from symbian.device.agent_identity import (
    key_file,
    pairing_code,
    read_identity,
)
from symbian.device.connection import ConnectedDevice
from symbian.status import Code, StatusError


def _phone(**updates):
    phone = ConnectedDevice(
        selector="usb:0421:05d1:test",
        manufacturer="Nokia",
        product="808 PureView",
        vendor_id=0x0421,
        product_id=0x05D1,
        location_id=1,
        identity_anchor="a" * 24,
        identity_basis="usb-serial",
    )
    return phone.model_copy(update=updates)


def test_phone_key_is_private_stable_and_requires_serial_anchor(
    monkeypatch, tmp_path
):
    monkeypatch.setattr(Path, "home", lambda: tmp_path)
    phone = _phone()
    key = key_file(phone, create=True)
    original = key.read_bytes()
    assert len(original) == 32
    assert stat.S_IMODE(key.stat().st_mode) == 0o600
    assert stat.S_IMODE(key.parent.stat().st_mode) == 0o700
    assert key_file(phone, create=True).read_bytes() == original
    code = pairing_code(phone)
    assert len(code) == 8
    assert set(code) <= set("ABCEGIKNOPRSTU")
    assert read_identity(phone, tmp_path / "project")["pairing_code"] == code
    with pytest.raises(StatusError) as error:
        key_file(_phone(identity_basis="unknown"), create=True)
    assert error.value.code == Code.FAILED_PRECONDITION


def test_broad_key_permissions_are_rejected(monkeypatch, tmp_path):
    monkeypatch.setattr(Path, "home", lambda: tmp_path)
    phone = _phone()
    key = key_file(phone, create=True)
    key.chmod(0o644)
    with pytest.raises(StatusError) as error:
        key_file(phone)
    assert error.value.code == Code.FAILED_PRECONDITION


def test_rotated_key_hides_package_built_for_previous_key(
    monkeypatch, tmp_path
):
    monkeypatch.setattr(Path, "home", lambda: tmp_path)
    phone = _phone()
    key = key_file(phone, create=True)
    project = tmp_path / "project"
    output = project / ".symbian/phone-agents" / phone.identity_anchor
    package = output / "package/agent_service.sis"
    package.parent.mkdir(parents=True)
    package.write_bytes(b"example package")
    (output / "profile.json").write_text(
        json.dumps(
            {
                "schema": "symbian.agent-profile/v1",
                "key_sha256": hashlib.sha256(key.read_bytes()).hexdigest(),
                "package_sha256": hashlib.sha256(
                    package.read_bytes()
                ).hexdigest(),
            }
        )
    )
    assert read_identity(phone, project)["package"] == str(package)
    key.write_bytes(bytes(32))
    assert read_identity(phone, project)["package"] is None
