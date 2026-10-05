"""Installs separately versioned, SDK-compatible emulator distributions."""

import hashlib
import json
import os
import platform
import re
import subprocess
import tarfile
import tempfile
from pathlib import Path
from typing import Literal

import httpx
from pydantic import BaseModel, ConfigDict, ValidationError

from symbian.emulator.configuration import atomic_json, xdg
from symbian.status import Code, StatusError

REPOSITORY = "hpnkv/symbian-platform"
PROTOCOL = "symbian.emulator-control/v1"
REQUIRED = {
    "isolated-data-root",
    "control-status",
    "framebuffer-capture",
    "pointer-input",
    "guest-exit-record",
    "firmware-import",
    "loopback-gdb",
    "dynarmic",
    "dyncom",
}
VERSION = re.compile(r"[0-9]+\.[0-9]+\.[0-9]+\Z")


class Distribution(BaseModel):
    """Only installation and compatibility metadata; no source inventory."""

    model_config = ConfigDict(extra="forbid")
    schema_: Literal["symbian.emulator-distribution/v1"] = (
        "symbian.emulator-distribution/v1"
    )
    version: str
    system: Literal["macos", "linux"]
    architecture: Literal["arm64", "aarch64", "x86_64"]
    frontend: Path
    importer: Path
    control_protocol: str
    capabilities: list[str]


def host() -> tuple[str, str]:
    """Returns the native asset OS/CPU, refusing unsupported hosts."""
    system = {"Darwin": "macos", "Linux": "linux"}.get(platform.system())
    arch = platform.machine()
    if system is None or arch not in {"arm64", "aarch64", "x86_64"}:
        raise StatusError(Code.FAILED_PRECONDITION, "Unsupported emulator host")
    if arch in {"arm64", "aarch64"}:
        arch = "arm64" if system == "macos" else "aarch64"
    return system, arch


def root() -> Path:
    """Keeps executable installations separate from preserved firmware."""
    return xdg("DATA") / "symbian/emulators"


def active_path() -> Path:
    """Returns the atomic per-user emulator selector."""
    return xdg("CONFIG") / "symbian/active-emulator.json"


def compatible(protocol: str, capabilities: list[str]) -> None:
    """Checks the SDK contract independently of package-version equality."""
    missing = REQUIRED - set(capabilities)
    if protocol != PROTOCOL or missing:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Emulator needs {PROTOCOL}; "
            f"missing capabilities: {sorted(missing)}",
        )


def read(prefix: Path) -> Distribution:
    """Validates declared tools stay inside a complete native installation."""
    try:
        value = json.loads((prefix / "emulator.json").read_text())
        value["schema_"] = value.pop("schema")
        declaration = Distribution.model_validate(value)
    except (OSError, ValueError, KeyError, ValidationError) as error:
        raise StatusError(
            Code.INVALID_ARGUMENT, f"Invalid emulator bundle: {error}"
        ) from error
    if not VERSION.fullmatch(declaration.version):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid emulator version")
    if (declaration.system, declaration.architecture) != host():
        raise StatusError(
            Code.FAILED_PRECONDITION, "Emulator archive is for another host"
        )
    compatible(declaration.control_protocol, declaration.capabilities)
    for relative in (declaration.frontend, declaration.importer):
        tool = (prefix / relative).resolve()
        if relative.is_absolute() or not tool.is_relative_to(prefix.resolve()):
            raise StatusError(
                Code.INVALID_ARGUMENT, "Emulator tool escapes its installation"
            )
        if not tool.is_file() or not os.access(tool, os.X_OK):
            raise StatusError(
                Code.INVALID_ARGUMENT, f"Missing executable: {relative}"
            )
    if not (prefix / "licenses").is_dir():
        raise StatusError(
            Code.INVALID_ARGUMENT, "Emulator licenses are missing"
        )
    return declaration


def query(frontend: Path) -> dict:
    """Asks the actual executable without starting Qt or touching firmware."""
    try:
        result = subprocess.run(
            [str(frontend), "--symbian-sdk-capabilities"],
            capture_output=True,
            text=True,
            timeout=10,
            check=True,
        )
        value = json.loads(result.stdout)
        if (
            not isinstance(value, dict)
            or value.get("schema") != "symbian.emulator-capabilities/v1"
        ):
            raise ValueError("Unexpected compatibility response")
        if not isinstance(value.get("capabilities"), list) or not all(
            isinstance(item, str) for item in value["capabilities"]
        ):
            raise ValueError("Invalid executable capabilities")
        compatible(value["control_protocol"], value["capabilities"])
        if not VERSION.fullmatch(value["version"]):
            raise ValueError("Invalid executable version")
        return value
    except (
        OSError,
        subprocess.SubprocessError,
        ValueError,
        KeyError,
        TypeError,
    ) as error:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Emulator compatibility check failed: {error}",
        ) from error


def check(prefix: Path) -> Distribution:
    """Checks metadata against the actual delivered executable."""
    declaration = read(prefix)
    actual = query(prefix / declaration.frontend)
    if actual["version"] != declaration.version:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Emulator executable version differs from bundle",
        )
    return declaration


def check_packaged(frontend: Path) -> None:
    """Rechecks bundled frontends before launch, including explicit choices."""
    executable = frontend.resolve()
    for prefix in executable.parents:
        if (prefix / "emulator.json").is_file():
            declaration = check(prefix)
            if (prefix / declaration.frontend).resolve() != executable:
                raise StatusError(
                    Code.FAILED_PRECONDITION,
                    "Selected executable is not the bundle frontend",
                )
            return


def active() -> dict[str, Path]:
    """Resolves the selected installation as the lowest preference layer."""
    path = active_path()
    if not path.is_file():
        return {}
    try:
        prefix = Path(json.loads(path.read_text())["prefix"])
    except (OSError, ValueError, KeyError, TypeError) as error:
        raise StatusError(
            Code.INVALID_ARGUMENT, f"Invalid emulator selector: {error}"
        ) from error
    declaration = read(prefix)
    return {
        "emulator": prefix / declaration.frontend,
        "importer": prefix / declaration.importer,
    }


def select(version: str) -> dict:
    """Checks and atomically selects an installed version, enabling rollback."""
    if not VERSION.fullmatch(version):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Expected an emulator X.Y.Z version"
        )
    system, architecture = host()
    prefix = root() / f"{version}-{system}-{architecture}"
    declaration = check(prefix)
    atomic_json(active_path(), {"prefix": str(prefix)})
    return {
        "version": declaration.version,
        "prefix": str(prefix),
        "selected": True,
    }


def released(version: str | None) -> dict:
    """Finds published emulator assets, independent of SDK and PyPI releases."""
    if version is not None and not VERSION.fullmatch(version):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Expected an emulator X.Y.Z version"
        )
    system, architecture = host()
    endpoint = f"https://api.github.com/repos/{REPOSITORY}/releases"
    try:
        with httpx.Client(timeout=30, follow_redirects=True) as client:
            candidates = []
            for page in range(1, 11):
                response = client.get(
                    endpoint, params={"per_page": 100, "page": page}
                )
                response.raise_for_status()
                releases = response.json()
                for release in releases:
                    tag = release["tag_name"]
                    candidate = tag.removeprefix("emulator-v")
                    if (
                        not tag.startswith("emulator-v")
                        or not VERSION.fullmatch(candidate)
                        or release["draft"]
                        or release["prerelease"]
                        or (version is not None and candidate != version)
                    ):
                        continue
                    filename = (
                        f"symbian-emulator-{candidate}-"
                        f"{system}-{architecture}.tar.gz"
                    )
                    asset = next(
                        (
                            item
                            for item in release["assets"]
                            if item["name"] == filename
                        ),
                        None,
                    )
                    if asset:
                        candidates.append((candidate, release, asset))
                if len(releases) < 100:
                    break
            candidates.sort(
                key=lambda item: tuple(map(int, item[0].split("."))),
                reverse=True,
            )
            for candidate, release, asset in candidates:
                metadata = next(
                    (
                        item
                        for item in release["assets"]
                        if item["name"] == f"symbian-emulator-{candidate}.json"
                    ),
                    None,
                )
                if metadata is None:
                    continue
                response = client.get(metadata["browser_download_url"])
                response.raise_for_status()
                declaration = response.json()
                if (
                    declaration.get("schema") != "symbian.emulator-release/v1"
                    or declaration.get("version") != candidate
                ):
                    continue
                try:
                    compatible(
                        declaration["control_protocol"],
                        declaration["capabilities"],
                    )
                except StatusError:
                    continue
                return asset
    except (httpx.HTTPError, ValueError, KeyError, TypeError) as error:
        raise StatusError(
            Code.UNAVAILABLE, f"Cannot discover emulator release: {error}"
        ) from error
    raise StatusError(
        Code.NOT_FOUND,
        "No matching published emulator; use --archive for an offline bundle",
    )


def install(
    *,
    archive: Path | None = None,
    version: str | None = None,
    sha256: str | None = None,
) -> dict:
    """Stages, checks, then selects a version without modifying old installs."""
    if sha256 is not None and not re.fullmatch(r"[0-9a-fA-F]{64}", sha256):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Expected a SHA-256 hex digest"
        )
    # An already selected compatible version does not need an upgrade merely
    # because the SDK has changed. Explicit versions still allow upgrades.
    if archive is None and version is None and active_path().is_file():
        try:
            tools = active()
            value = query(tools["emulator"])
            return {
                "version": value["version"],
                "selected": True,
                "reused": True,
            }
        except StatusError:
            # A new SDK contract may require an upgrade. Preserve the previous
            # selection until a compatible downloaded executable passes checks.
            pass
    base = root()
    base.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".install-", dir=base) as temporary:
        staging = Path(temporary)
        if archive is None:
            asset = released(version)
            archive = staging / "download.tar.gz"
            advertised = asset.get("digest", "")
            if advertised and not advertised.startswith("sha256:"):
                raise StatusError(
                    Code.INVALID_ARGUMENT, "Unsupported release digest"
                )
            sha256 = sha256 or advertised.removeprefix("sha256:") or None
            try:
                with httpx.stream(
                    "GET",
                    asset["browser_download_url"],
                    follow_redirects=True,
                    timeout=60,
                ) as response:
                    response.raise_for_status()
                    with archive.open("wb") as stream:
                        for block in response.iter_bytes():
                            stream.write(block)
                            if stream.tell() > 2 * 1024**3:
                                raise StatusError(
                                    Code.RESOURCE_EXHAUSTED,
                                    "Emulator download exceeds 2 GiB",
                                )
            except httpx.HTTPError as error:
                raise StatusError(
                    Code.UNAVAILABLE, f"Emulator download failed: {error}"
                ) from error
        if sha256 is not None:
            with archive.open("rb") as stream:
                actual = hashlib.file_digest(stream, "sha256").hexdigest()
            if actual != sha256.lower():
                raise StatusError(
                    Code.INVALID_ARGUMENT, "Emulator archive digest differs"
                )
        payload = staging / "payload"
        payload.mkdir()
        try:
            with tarfile.open(archive) as bundle:
                if (
                    sum(member.size for member in bundle.getmembers())
                    > 4 * 1024**3
                ):
                    raise StatusError(
                        Code.RESOURCE_EXHAUSTED,
                        "Expanded emulator exceeds 4 GiB",
                    )
                bundle.extractall(payload, filter="data")
        except (OSError, tarfile.TarError) as error:
            raise StatusError(
                Code.INVALID_ARGUMENT, f"Invalid emulator archive: {error}"
            ) from error
        declaration = check(payload)
        if version is not None and declaration.version != version:
            raise StatusError(
                Code.INVALID_ARGUMENT,
                "Archive does not match requested emulator version",
            )
        destination = base / (
            f"{declaration.version}-{declaration.system}-"
            f"{declaration.architecture}"
        )
        if destination.exists():
            check(destination)
        else:
            payload.rename(destination)
        return select(declaration.version)


def installed() -> dict:
    """Lists retained versions without executing every installation."""
    selected = active_path().read_text() if active_path().is_file() else ""
    versions = []
    if root().is_dir():
        for prefix in sorted(root().iterdir()):
            if prefix.name.startswith("."):
                continue
            declaration = read(prefix)
            versions.append(
                {
                    "version": declaration.version,
                    "prefix": str(prefix),
                    "selected": str(prefix) in selected,
                }
            )
    return {"installations": versions}
