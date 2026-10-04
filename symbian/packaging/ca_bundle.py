"""Validate and locate a project's explicitly selected CA bundle."""

import hashlib
import re
import ssl
import subprocess
from pathlib import Path

from symbian.status import Code, StatusError

_MAX_BUNDLE = 256 * 1024
_CERTIFICATE = re.compile(
    rb"-----BEGIN CERTIFICATE-----\s+"
    rb"[A-Za-z0-9+/=\r\n]+"
    rb"-----END CERTIFICATE-----"
)


def selected_bundle(project: Path, artifact: Path) -> tuple[Path, bytes] | None:
    """Read the CMake selection and validate every PEM certificate.

    Args:
        project: Root containing the application's CMakeLists.txt.
        artifact: ELF produced in the configured CMake build tree.

    Returns:
        The project PEM path and contents, or None when unset.
    """
    cache = artifact.parent / "cmake/CMakeCache.txt"
    if not cache.is_file():
        cache = artifact.parent / "CMakeCache.txt"
    if not cache.is_file():
        return None
    values = {}
    for line in cache.read_text(encoding="utf-8").splitlines():
        if line.startswith(("CMAKE_HOME_DIRECTORY:", "SYMBIAN_CA_BUNDLE:")):
            key, value = line.split("=", 1)
            values[key.split(":", 1)[0]] = value
    choice = values.get("SYMBIAN_CA_BUNDLE", "")
    if not choice:
        return None
    if Path(values.get("CMAKE_HOME_DIRECTORY", "")).resolve() != project:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "CA bundle CMake cache belongs to another project",
        )
    source = Path(choice).resolve()
    if not source.is_relative_to(project) or not source.is_file():
        raise StatusError(
            Code.INVALID_ARGUMENT, "CA bundle must be a project file"
        )
    with source.open("rb") as stream:
        data = stream.read(_MAX_BUNDLE + 1)
    if not data or len(data) > _MAX_BUNDLE:
        raise StatusError(
            Code.RESOURCE_EXHAUSTED, "CA bundle must be 1..262144 bytes"
        )
    certificates = list(_CERTIFICATE.finditer(data))
    if not certificates:
        raise StatusError(Code.INVALID_ARGUMENT, "CA bundle has no certificate")
    residue = _CERTIFICATE.sub(b"", data)
    if residue.strip():
        raise StatusError(
            Code.INVALID_ARGUMENT, "CA bundle contains non-certificate data"
        )
    for match in certificates:
        try:
            ssl.PEM_cert_to_DER_cert(match.group().decode("ascii"))
            checked = subprocess.run(
                ["openssl", "x509", "-inform", "PEM", "-noout"],
                input=match.group(),
                capture_output=True,
                check=False,
                timeout=5,
            )
        except (
            OSError,
            UnicodeError,
            ValueError,
            subprocess.TimeoutExpired,
        ) as error:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Invalid CA bundle certificate"
            ) from error
        if checked.returncode:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Invalid CA bundle certificate"
            )
    return source, data


def digest(data: bytes) -> str:
    """Return the SHA-256 digest recorded with the package."""
    return hashlib.sha256(data).hexdigest()
