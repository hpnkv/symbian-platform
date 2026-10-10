"""Generic agent packages and separately paired per-device credentials.

The installed agent creates its secret in its private phone storage. A host
record can hold that secret only after a verified pairing exchange. Legacy
phone-bound records remain readable for already installed agents.
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
from symbian.paths import asset_directory
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
    return asset_directory("data") / "agent-identities" / _anchor(device)


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
    """Describe the generic package and any established phone pairing."""
    package_path = project / ".symbian/agent-package/package/agent_service.sis"
    metadata_path = project / ".symbian/agent-package/profile.json"
    package = None
    try:
        if metadata_path.stat().st_size > 4096:
            raise ValueError("Agent package record is too large")
        if package_path.stat().st_size > 16 * 1024 * 1024:
            raise ValueError("Agent package is too large")
        metadata = json.loads(metadata_path.read_text(encoding="utf-8"))
        if (
            metadata["schema"] == "symbian.agent-package/v3"
            and metadata["package_sha256"]
            == hashlib.sha256(package_path.read_bytes()).hexdigest()
        ):
            package = str(package_path)
    except (OSError, ValueError, KeyError, TypeError):
        pass
    code = None
    if (identity_dir(device) / "agent.key").exists():
        try:
            code = pairing_code(device)
        except (OSError, StatusError):
            pass
    if package is None and code is None:
        return None
    return {
        "package": package,
        "pairing_code": code,
        "anchor": _anchor(device),
    }


def build_package(
    device: ConnectedDevice, sdk_manifest: Path, project: Path
) -> dict:
    """Build one reusable SIS with no phone secret or selected-phone setting."""
    sdk = AppSdk.load(sdk_manifest)
    output = project / ".symbian/agent-package"
    output.parent.mkdir(parents=True, exist_ok=True, mode=0o700)
    output.mkdir(parents=True, exist_ok=True, mode=0o700)
    built = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
        cmake_variables={"SYMBIAN_EMULATOR_RM807_AGENT": "OFF"},
    )
    certificate, private_key = create_self_signed_identity(
        asset_directory("data") / "agent-signing",
        "Symbian Development Agent",
    )
    packaged = packaging.package(
        project,
        Path(built["artifact"]),
        output / "package",
        signing_certificate=certificate,
        signing_key=private_key,
    )
    profile = {
        "schema": "symbian.agent-package/v3",
        "package_sha256": packaged["sha256"],
    }
    profile_path = output / "profile.json"
    profile_path.write_text(
        json.dumps(profile, sort_keys=True) + "\n", encoding="utf-8"
    )
    profile_path.chmod(0o600)
    return {
        "package": packaged["artifact"],
        "pairing_code": None,
        "profile": "generic-unpaired",
        "anchor": _anchor(device),
    }
