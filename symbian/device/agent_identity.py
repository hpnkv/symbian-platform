"""Private per-phone credentials and packages for the resident agent.

USB establishes a stable local identity anchor, not a network connection.
The secret remains in user-local storage and is embedded only in that phone's
agent executable. An authenticated network reply still needs owner pairing to
associate a Wi-Fi endpoint with the phone seen over USB.
"""

from __future__ import annotations

import hashlib
import json
import os
import secrets
import stat
from pathlib import Path

from symbian import packaging, toolchain
from symbian.device.connection import ConnectedDevice
from symbian.packaging.signing import create_self_signed_identity
from symbian.project.sdk import AppSdk
from symbian.status import Code, StatusError


def _anchor(device: ConnectedDevice) -> str:
    anchor = device.identity_anchor
    if (
        device.identity_basis != "usb-serial"
        or len(anchor) != 24
        or any(ch not in "0123456789abcdef" for ch in anchor)
    ):
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "A serial-derived phone identity is required",
        )
    return anchor


def identity_dir(device: ConnectedDevice) -> Path:
    """Return private storage for one serial-derived phone anchor."""
    return (
        Path.home() / ".local/share/symbian/agent-identities" / _anchor(device)
    )


def key_file(device: ConnectedDevice, *, create: bool = False) -> Path:
    """Find or create a private 32-byte key, refusing broad permissions."""
    directory = identity_dir(device)
    path = directory / "agent.key"
    if create:
        directory.mkdir(parents=True, exist_ok=True, mode=0o700)
        directory.chmod(0o700)
        try:
            flags = os.O_WRONLY | os.O_CREAT | os.O_EXCL | os.O_NOFOLLOW
            descriptor = os.open(path, flags, 0o600)
        except FileExistsError:
            pass
        else:
            with os.fdopen(descriptor, "wb") as stream:
                stream.write(secrets.token_bytes(32))
                stream.flush()
                os.fsync(stream.fileno())
    info = path.lstat()
    if (
        not stat.S_ISREG(info.st_mode)
        or info.st_mode & 0o077
        or path.stat().st_size != 32
    ):
        raise StatusError(Code.FAILED_PRECONDITION, "Unsafe agent key file")
    return path


def pairing_code(device: ConnectedDevice) -> str:
    """Short public fingerprint for the owner to compare on the handset."""
    alphabet = "ABCEGIKNOPRSTU"
    value = int.from_bytes(
        hashlib.sha256(key_file(device).read_bytes()).digest()[:4], "big"
    )
    characters = []
    for _ in range(8):
        value, index = divmod(value, len(alphabet))
        characters.append(alphabet[index])
    return "".join(reversed(characters))


def read_identity(device: ConnectedDevice, project: Path) -> dict | None:
    """Describe an existing phone identity without creating credentials."""
    if not (identity_dir(device) / "agent.key").exists():
        return None
    try:
        code = pairing_code(device)
    except (OSError, StatusError):
        return None
    output = project / ".symbian/phone-agents" / _anchor(device)
    package_path = output / "package/agent_service.sis"
    metadata_path = output / "profile.json"
    package = None
    try:
        if metadata_path.stat().st_size > 4096:
            raise ValueError("Agent profile record is too large")
        if package_path.stat().st_size > 16 * 1024 * 1024:
            raise ValueError("Agent package is too large")
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        if (
            metadata["schema"] == "symbian.agent-profile/v2"
            and metadata["key_sha256"]
            == hashlib.sha256(key_file(device).read_bytes()).hexdigest()
            and metadata["package_sha256"]
            == hashlib.sha256(package_path.read_bytes()).hexdigest()
        ):
            package = str(package_path)
    except (OSError, ValueError, KeyError, TypeError):
        pass
    return {
        "package": package,
        "pairing_code": code,
        "anchor": _anchor(device),
    }


def build_package(
    device: ConnectedDevice, sdk_manifest: Path, project: Path
) -> dict:
    """Build a phone-specific SIS using the selected SDK and private key."""
    sdk = AppSdk.load(sdk_manifest)
    key = key_file(device, create=True)
    output = project / ".symbian/phone-agents" / _anchor(device)
    output.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    output.parent.chmod(0o700)
    output.mkdir(parents=True, exist_ok=True, mode=0o700)
    output.chmod(0o700)
    built = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
        cmake_variables={
            "SYMBIAN_AGENT_PRIVATE_KEY_FILE": str(key),
            "SYMBIAN_AGENT_KEY_SHA256": hashlib.sha256(
                key.read_bytes()
            ).hexdigest(),
            "SYMBIAN_AGENT_PAIRING_CODE": pairing_code(device),
        },
    )
    certificate, private_key = create_self_signed_identity(
        identity_dir(device), "Symbian Development Agent"
    )
    packaged = packaging.package(
        project,
        Path(built["artifact"]),
        output / "package",
        signing_certificate=certificate,
        signing_key=private_key,
    )
    profile = {
        "schema": "symbian.agent-profile/v2",
        "key_sha256": hashlib.sha256(key.read_bytes()).hexdigest(),
        "package_sha256": packaged["sha256"],
    }
    profile_path = output / "profile.json"
    profile_path.write_text(
        json.dumps(profile, sort_keys=True) + "\n", encoding="utf-8"
    )
    profile_path.chmod(0o600)
    return {
        "package": packaged["artifact"],
        "pairing_code": pairing_code(device),
        "profile": "phone-specific-unverified",
        "anchor": _anchor(device),
    }
