"""Prepare SDK packages and stage them through a proven USB storage volume."""

import hashlib
import os
import shutil
import tempfile
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict

from symbian.device.connection import select
from symbian.packaging import inspect_package, package
from symbian.status import Code, StatusError


class InstallResult(BaseModel):
    """Transfer state, with explicit separation from installer verification."""

    model_config = ConfigDict(frozen=True)

    schema_name: Literal["symbian.device-install/v1"]
    state: Literal["awaiting-on-device-install"]
    transport: Literal["usb-mass-storage"]
    device: str
    volume: str
    staged_path: Path
    sha256: str
    copied: bool
    package: dict
    on_device_verified: bool = False
    next_action: str
    build_artifact: Path | None = None
    host_package: Path | None = None
    build_sha256: str | None = None


def _digest(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as stream:
        for block in iter(lambda: stream.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def prepare_package(
    project: Path, compiler: str = "clang++", linker: str = "ld.lld"
) -> dict:
    """Builds an executable and SIS with the project's selected SDK."""
    from symbian import toolchain

    project = project.resolve()
    if (project / "symbian-project.json").is_file():
        from symbian.project.configuration import ProjectConfiguration

        sdk = ProjectConfiguration.load(project).sdk
        compiler, linker = str(sdk.compiler), str(sdk.linker)
    build = toolchain.build(
        project, project / ".symbian/build", compiler, linker
    )
    if build.get("artifact_kind") != "experimental-e32-executable":
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Device installation requires an E32 executable project",
        )
    built = Path(build["artifact"])
    if _digest(built) != build["sha256"]:
        raise StatusError(
            Code.ABORTED, "Built executable changed before packaging"
        )
    packaged = package(project, built, project / ".symbian/package")
    return {"build": build, "package": packaged}


def stage_package(
    package_path: Path, selector: str | None = None, disk: str | None = None
) -> dict:
    """Copies a checked SIS; a human must run its installer on the handset."""
    package_path = package_path.resolve(strict=True)
    expected = _digest(package_path)
    metadata = inspect_package(package_path)
    if _digest(package_path) != expected:
        raise StatusError(Code.ABORTED, "Package changed during inspection")
    device = select(selector)
    volumes = [
        volume
        for volume in device.volumes
        if volume.stage_sis and (disk is None or volume.disk == disk)
    ]
    if not volumes:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "No writable Installs volume; use mass-storage mode",
        )
    if len(volumes) != 1:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "Several volumes match; pass --volume from device info",
        )
    volume = volumes[0]
    installs = volume.mount / "Installs"
    if not installs.is_dir() or installs.is_symlink():
        raise StatusError(Code.ABORTED, "Phone's Installs directory changed")
    name = Path(str(metadata["executable_name"])).stem
    if not name or not all(
        char.isascii() and (char.isalnum() or char in "_-") for char in name
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Unsafe executable name in SIS"
        )
    destination = installs / f"{name}-{expected[:12]}.sis"
    if destination.exists():
        try:
            matches = (
                not destination.is_symlink()
                and _digest(destination) == expected
            )
        except OSError as error:
            raise StatusError(
                Code.UNAVAILABLE, "Phone volume unavailable during verification"
            ) from error
        if not matches:
            raise StatusError(
                Code.ALREADY_EXISTS,
                "A different package already occupies the intended phone path",
            )
        copied = False
    else:
        try:
            available = os.statvfs(installs)
        except OSError as error:
            raise StatusError(
                Code.UNAVAILABLE, "Phone volume disconnected before transfer"
            ) from error
        if (
            available.f_bavail * available.f_frsize
            < package_path.stat().st_size + 16 * 1024 * 1024
        ):
            raise StatusError(
                Code.RESOURCE_EXHAUSTED,
                "Phone volume needs package size plus 16 MiB free space",
            )
        temporary: Path | None = None
        try:
            with tempfile.NamedTemporaryFile(
                mode="wb",
                prefix=".symbian-",
                suffix=".part",
                dir=installs,
                delete=False,
            ) as stream:
                temporary = Path(stream.name)
                with package_path.open("rb") as source:
                    shutil.copyfileobj(source, stream, length=1024 * 1024)
                stream.flush()
                os.fsync(stream.fileno())
            if (
                _digest(temporary) != expected
                or _digest(package_path) != expected
            ):
                raise StatusError(
                    Code.ABORTED, "Package changed during transfer"
                )
            # The content-addressed name makes a conflicting replacement
            # unlikely; check again before publishing a complete file.
            if destination.exists():
                raise StatusError(
                    Code.ALREADY_EXISTS, "Phone path appeared during transfer"
                )
            temporary.replace(destination)
            copied = True
        except OSError as error:
            raise StatusError(
                Code.UNAVAILABLE, "Phone volume unavailable during transfer"
            ) from error
        finally:
            if temporary is not None:
                try:
                    temporary.unlink(missing_ok=True)
                except OSError:
                    pass
    try:
        copied_hash = _digest(destination)
    except OSError as error:
        raise StatusError(
            Code.UNAVAILABLE, "Phone volume unavailable during verification"
        ) from error
    if copied_hash != expected:
        raise StatusError(
            Code.DATA_LOSS, "Phone copy failed content verification"
        )
    result = InstallResult(
        schema_name="symbian.device-install/v1",
        state="awaiting-on-device-install",
        transport="usb-mass-storage",
        device=device.selector,
        volume=volume.disk,
        staged_path=destination,
        sha256=expected,
        copied=copied,
        package=metadata,
        next_action=(
            "Finish copying files, safely eject the USB volume, then open "
            "the SIS in the phone's Installs folder and accept its prompts. "
            + (
                "This package includes application-menu registration. "
                if metadata.get("application_registered")
                else "This package has no application-menu registration. "
            )
            + "The SDK has not verified installation or execution."
        ),
    )
    return {
        "schema": result.schema_name,
        **result.model_dump(mode="json", exclude={"schema_name"}),
    }


def install(
    project: Path,
    selector: str | None = None,
    disk: str | None = None,
    *,
    package_path: Path | None = None,
    compiler: str = "clang++",
    linker: str = "ld.lld",
) -> dict:
    """Builds/packages by default, then stages for the handset installer."""
    prepared = (
        None
        if package_path is not None
        else prepare_package(project, compiler, linker)
    )
    source = (
        package_path
        if package_path is not None
        else Path(prepared["package"]["artifact"])
    )
    result = stage_package(source, selector, disk)
    if prepared is not None:
        result["build_artifact"] = prepared["build"]["artifact"]
        result["host_package"] = prepared["package"]["artifact"]
        result["build_sha256"] = prepared["build"]["sha256"]
    return result
