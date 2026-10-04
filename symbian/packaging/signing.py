"""Host policy for reusable, locally held SIS signing identities."""

from __future__ import annotations

import os
import shutil
import stat
import subprocess
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


__all__ = ["create_self_signed_identity"]
