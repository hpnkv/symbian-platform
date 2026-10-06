"""Host policy for reusable, locally held SIS signing identities."""

from __future__ import annotations

import hashlib
import os
import re
import shutil
import stat
import subprocess
import tempfile
import uuid
from pathlib import Path

from symbian.status import Code, StatusError


def create_self_signed_identity(
    directory: Path, common_name: str
) -> tuple[Path, Path]:
    """Create or reuse an RSA signing identity in a private directory.

    Args:
        directory: User-owned location outside the project and source control.
        common_name: Printable certificate name shown to an installer.

    Returns:
        PEM certificate and private-key paths for ``package``.
    """
    if (
        not 1 <= len(common_name) <= 64
        or any(ord(char) < 32 or ord(char) > 126 for char in common_name)
        or any(char in common_name for char in "/=\\")
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid signing common name")
    directory.mkdir(parents=True, exist_ok=True, mode=0o700)
    info = directory.lstat()
    if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid():
        raise StatusError(Code.FAILED_PRECONDITION, "Unsafe signing directory")
    directory.chmod(0o700)
    certificate = directory / "signing.cer"
    private_key = directory / "signing.key"
    if certificate.exists() != private_key.exists():
        raise StatusError(
            Code.FAILED_PRECONDITION, "Incomplete signing identity"
        )
    if not certificate.exists():
        openssl = shutil.which("openssl")
        if openssl is None:
            raise StatusError(Code.FAILED_PRECONDITION, "OpenSSL is required")
        result = subprocess.run(
            [
                openssl,
                "req",
                "-x509",
                "-newkey",
                "rsa:2048",
                "-sha256",
                "-nodes",
                "-days",
                "3650",
                "-subj",
                f"/CN={common_name}",
                "-keyout",
                str(private_key),
                "-out",
                str(certificate),
            ],
            capture_output=True,
            check=False,
        )
        if result.returncode != 0:
            raise StatusError(
                Code.INTERNAL, "Could not create signing identity"
            )
        private_key.chmod(0o600)
        certificate.chmod(0o600)
    for path in (private_key, certificate):
        info = path.lstat()
        if not stat.S_ISREG(info.st_mode) or info.st_mode & 0o077:
            raise StatusError(Code.FAILED_PRECONDITION, "Unsafe signing file")
    return certificate, private_key


def _openssl(arguments: list[str], data: bytes) -> bytes:
    """Run a bounded identity check without exposing private input or errors."""
    executable = shutil.which("openssl")
    if executable is None:
        raise StatusError(Code.FAILED_PRECONDITION, "OpenSSL is required")
    try:
        result = subprocess.run(
            [executable, *arguments],
            input=data,
            capture_output=True,
            timeout=30,
            check=False,
        )
    except subprocess.TimeoutExpired as error:
        raise StatusError(
            Code.DEADLINE_EXCEEDED, "Signing identity check timed out"
        ) from error
    if result.returncode:
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Expected a PEM certificate and matching unencrypted RSA key",
        )
    return result.stdout


def _check_identity(certificate: bytes, private_key: bytes) -> dict:
    """Validate the RSA pair and return only public certificate details."""
    public_certificate = _openssl(["x509", "-pubkey", "-noout"], certificate)
    public_key = _openssl(["rsa", "-pubout", "-passin", "pass:"], private_key)
    if public_certificate != public_key:
        raise StatusError(Code.INVALID_ARGUMENT, "Certificate and key differ")
    details = _openssl(
        ["x509", "-noout", "-subject", "-enddate", "-fingerprint", "-sha256"],
        certificate,
    ).decode("utf-8", errors="replace")
    fields = dict(
        line.split("=", 1) for line in details.splitlines() if "=" in line
    )
    return {
        "subject": fields.get("subject", ""),
        "expires_at": fields.get("notAfter", ""),
        "fingerprint_sha256": next(
            (value for key, value in fields.items() if "Fingerprint" in key),
            "",
        ),
    }


class IdentityStore:
    """Private named identities stored outside application source trees."""

    def __init__(self, directory: Path | None = None):
        from symbian.paths import asset_directory

        self.directory = (
            directory.expanduser().absolute()
            if directory is not None
            else asset_directory("data") / "signing"
        )

    def _private_directory(self, path: Path) -> None:
        path.mkdir(parents=True, exist_ok=True, mode=0o700)
        info = path.lstat()
        if not stat.S_ISDIR(info.st_mode) or info.st_uid != os.getuid():
            raise StatusError(
                Code.FAILED_PRECONDITION, "Unsafe signing directory"
            )
        path.chmod(0o700)

    def _path(self, name: str) -> Path:
        if not re.fullmatch(r"[A-Za-z0-9][A-Za-z0-9_-]{0,63}", name):
            raise StatusError(
                Code.INVALID_ARGUMENT,
                "Identity name needs 1–64 letters, digits, "
                "underscores or hyphens",
            )
        return self.directory / name

    def paths(self, name: str) -> tuple[Path, Path]:
        """Read one private identity without creating it."""
        path = self._path(name)
        if not path.exists():
            raise StatusError(
                Code.NOT_FOUND, f"Signing identity not found: {name}"
            )
        for item, directory in (
            (self.directory, True),
            (path, True),
            (path / "signing.cer", False),
            (path / "signing.key", False),
        ):
            info = item.lstat()
            valid_type = stat.S_ISDIR if directory else stat.S_ISREG
            if (
                not valid_type(info.st_mode)
                or info.st_uid != os.getuid()
                or info.st_mode & 0o077
            ):
                raise StatusError(
                    Code.FAILED_PRECONDITION, "Unsafe signing identity"
                )
        return path / "signing.cer", path / "signing.key"

    def _record(self, name: str) -> dict:
        from symbian.packaging.packages import _read_bounded

        certificate, key = self.paths(name)
        details = _check_identity(
            _read_bounded(certificate, 16 * 1024),
            _read_bounded(key, 16 * 1024),
        )
        return {
            "name": name,
            "certificate": str(certificate),
            "private_key": str(key),
            **details,
        }

    def list(self) -> dict:
        """List public metadata; private key bytes never leave this store."""
        if not self.directory.exists():
            return {"directory": str(self.directory), "identities": []}
        self._private_directory(self.directory)
        records = []
        for path in sorted(self.directory.iterdir()):
            if path.name.startswith("."):
                continue
            records.append(self._record(path.name))
        return {"directory": str(self.directory), "identities": records}

    def _publish(self, name: str, prepare) -> dict:
        target = self._path(name)
        self._private_directory(self.directory)
        if target.exists() or target.is_symlink():
            raise StatusError(
                Code.ALREADY_EXISTS, "Signing identity already exists"
            )
        with tempfile.TemporaryDirectory(
            prefix=".identity-", dir=self.directory
        ) as temporary:
            staged = Path(temporary) / "identity"
            self._private_directory(staged)
            prepare(staged)
            from symbian.packaging.packages import _read_bounded

            _check_identity(
                _read_bounded(staged / "signing.cer", 16 * 1024),
                _read_bounded(staged / "signing.key", 16 * 1024),
            )
            staged.rename(target)
        return self._record(name)

    def create(self, name: str, common_name: str) -> dict:
        """Create a fresh identity without replacing existing keys."""
        return self._publish(
            name,
            lambda staged: create_self_signed_identity(staged, common_name),
        )

    def import_identity(
        self, name: str, certificate: Path, private_key: Path
    ) -> dict:
        """Copy a matching PEM RSA pair into private host storage."""
        from symbian.packaging.packages import _read_bounded

        certificate_data = _read_bounded(certificate, 16 * 1024)
        key_data = _read_bounded(private_key, 16 * 1024)
        _check_identity(certificate_data, key_data)

        def prepare(staged: Path) -> None:
            for filename, data in (
                ("signing.cer", certificate_data),
                ("signing.key", key_data),
            ):
                path = staged / filename
                with path.open("xb") as stream:
                    path.chmod(0o600)
                    stream.write(data)

        return self._publish(name, prepare)

    def archive(self, name: str) -> dict:
        """Retire an identity while retaining its key material."""
        self.paths(name)
        archive = self.directory / ".archive"
        self._private_directory(archive)
        destination = archive / f"{name}-{uuid.uuid4().hex}"
        self._path(name).rename(destination)
        return {"name": name, "destination": str(destination), "archived": True}


def sign_package(
    package: Path, destination: Path, certificate: Path, private_key: Path
) -> dict:
    """Sign an existing SIS through the native signer into a new output file.

    Args:
        package: Existing unsigned SIS in the native signer's supported profile.
        destination: New signed SIS file; existing files are never replaced.
        certificate: PEM signing certificate.
        private_key: Matching unencrypted RSA key.

    Returns:
        Output path, SHA-256 and verified native SIS metadata.
    """
    from symbian.native import require_native
    from symbian.packaging.packages import (
        _MAX_PACKAGE,
        _read_bounded,
        inspect_package,
    )

    data = _read_bounded(package, _MAX_PACKAGE)
    certificate_data = _read_bounded(certificate, 16 * 1024)
    key_data = _read_bounded(private_key, 16 * 1024)
    signed = require_native().sign_sis(data, certificate_data, key_data)
    # Native verification must pass before publishing any bytes.
    require_native().inspect_sis(signed)
    destination = destination.expanduser().absolute()
    destination.parent.mkdir(parents=True, exist_ok=True)
    try:
        with destination.open("xb") as stream:
            stream.write(signed)
    except FileExistsError as error:
        raise StatusError(
            Code.ALREADY_EXISTS, "Signed output already exists"
        ) from error
    return {
        "artifact": str(destination),
        "signed": True,
        "sha256": hashlib.sha256(signed).hexdigest(),
        "sis": inspect_package(destination),
    }


__all__ = ["IdentityStore", "create_self_signed_identity", "sign_package"]
