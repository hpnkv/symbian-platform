"""Shared, content-addressed firmware baselines and native import policy."""

import fcntl
import hashlib
import json
import re
import shutil
import subprocess
import tempfile
from contextlib import contextmanager
from pathlib import Path
from typing import Literal

from pydantic import BaseModel, ConfigDict, ValidationError

from symbian.emulator.configuration import Resolution, atomic_json
from symbian.paths import asset_directory
from symbian.status import Code, StatusError

UPSTREAM = "2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8"
ROM_808 = "b5c1ea63cb6359270c5b7cfb1bb453594e208a01b8aeb5b5e020f37d546f7086"
EUSER_808 = "3cec7e1546f8ed0cf64a73fece9fdd8fe6e4976535ddffd18b7068c19c01357b"


def digest(path: Path) -> str:
    """Hashes a file in bounded memory."""
    with path.open("rb") as stream:
        return hashlib.file_digest(stream, "sha256").hexdigest()


def inventory(directory: Path) -> dict[str, str]:
    """Inventories regular files, rejecting links and unusual file types."""
    if directory.is_symlink() or not directory.is_dir():
        raise StatusError(
            Code.INVALID_ARGUMENT, f"Expected a real directory: {directory}"
        )
    result = {}
    for path in sorted(directory.rglob("*")):
        if path.is_symlink() or not (path.is_file() or path.is_dir()):
            raise StatusError(
                Code.INVALID_ARGUMENT, f"Unsupported filesystem entry: {path}"
            )
        if path.is_file():
            result[path.relative_to(directory).as_posix()] = digest(path)
    return result


class Device(BaseModel):
    """Device metadata discovered by EKA2L1 from an emulator dump."""

    model_config = ConfigDict(extra="forbid")
    firmware_code: str
    model: str
    manufacturer: str
    symbian_version: str
    kernel: Literal["eka1", "eka2"]
    machine_uid: int
    rom: str
    z_drive: str
    c_drive: str


class Manifest(BaseModel):
    """Portable identity excludes aliases, source paths and host tool paths."""

    model_config = ConfigDict(extra="forbid")
    schema_version: Literal[1] = 1
    identity: str
    device: Device
    files: dict[str, str]
    provenance: dict


class NativeStatus(BaseModel):
    """Canonical process-boundary outcome from the native importer."""

    code: Code
    message: str


class NativeResponse(BaseModel):
    """Validates the native response before interpreting it as policy data."""

    status: NativeStatus
    result: dict | None = None


def identity(device: Device, files: dict) -> str:
    """Identifies exactly the normalized device metadata and baseline files."""
    payload = {
        "schema_version": 1,
        "device": device.model_dump(),
        "files": files,
    }
    return hashlib.sha256(
        json.dumps(payload, sort_keys=True, separators=(",", ":")).encode()
    ).hexdigest()


def validate_manifest(directory: Path) -> Manifest:
    """Verifies all bytes and path mappings before a baseline can be used."""
    try:
        manifest = Manifest.model_validate_json(
            (directory / "firmware.json").read_text()
        )
    except (ValidationError, UnicodeError) as error:
        raise StatusError(
            Code.DATA_LOSS, f"Invalid firmware manifest: {directory}: {error}"
        ) from error
    actual = inventory(directory / "instance")
    if (
        actual != manifest.files
        or identity(manifest.device, actual) != manifest.identity
    ):
        raise StatusError(
            Code.DATA_LOSS, f"Firmware baseline integrity mismatch: {directory}"
        )
    for value in (
        manifest.device.rom,
        manifest.device.z_drive,
        manifest.device.c_drive,
    ):
        path = Path(value)
        if path.is_absolute() or ".." in path.parts or "\\" in value:
            raise StatusError(
                Code.DATA_LOSS, f"Unsafe firmware mapping: {value}"
            )
    if manifest.device.rom not in actual or not any(
        key.startswith(manifest.device.z_drive + "/") for key in actual
    ):
        raise StatusError(
            Code.DATA_LOSS, "Firmware has no mapped ROM / drive Z"
        )
    return manifest


@contextmanager
def locked(store: Path):
    """Serializes publication and alias edits with a file lock."""
    store.mkdir(parents=True, exist_ok=True)
    with (store / ".lock").open("a") as stream:
        fcntl.flock(stream, fcntl.LOCK_EX)
        yield


def aliases(store: Path) -> dict[str, str]:
    """Reads the local alias index; portable selections use SHA256 IDs."""
    path = store / "aliases.json"
    if not path.exists():
        return {}
    try:
        value = json.loads(path.read_text())
        if not isinstance(value, dict) or any(
            not isinstance(key, str)
            or not isinstance(item, str)
            or not re.fullmatch(r"[0-9a-f]{64}", item)
            for key, item in value.items()
        ):
            raise ValueError("Expected alias -> SHA256 mapping")
        return value
    except (ValueError, UnicodeError) as error:
        raise StatusError(Code.DATA_LOSS, f"{path}: {error}") from error


def locate(store: Path, reference: str) -> Path:
    """Resolves an alias or exact portable ID without guessing device names."""
    value = (
        reference.removeprefix("sha256:")
        if reference.startswith("sha256:")
        else aliases(store).get(reference)
    )
    if value is None or not re.fullmatch(r"[0-9a-f]{64}", value):
        raise StatusError(
            Code.NOT_FOUND,
            f"Firmware {reference!r} is not in {store}; use 'symbian firmware"
            " list' or import it",
        )
    directory = store / "objects" / value
    if not directory.is_dir():
        raise StatusError(
            Code.NOT_FOUND, f"Firmware object missing: {directory}"
        )
    return directory


def native_request(
    arguments: list[str], directory: Path, retained: Path, timeout: float
) -> dict:
    """Runs and reaps a synchronous native reader with retained evidence."""
    with (retained / "native.log").open("w") as log:
        try:
            result = subprocess.run(
                arguments,
                cwd=directory,
                stdout=subprocess.PIPE,
                stderr=log,
                text=True,
                timeout=timeout,
                check=False,
            )
        except subprocess.TimeoutExpired as error:
            raise StatusError(
                Code.DEADLINE_EXCEEDED,
                f"Native import timed out; evidence: {retained}",
            ) from error
    (retained / "native-response.txt").write_text(result.stdout)
    try:
        envelope = NativeResponse.model_validate_json(result.stdout)
        if envelope.status.code == Code.OK and envelope.result is None:
            raise ValueError("Successful native response lacks result")
        response = envelope.model_dump(mode="json")
        status = response["status"]
    except (ValueError, KeyError, TypeError) as error:
        raise StatusError(
            Code.DATA_LOSS,
            f"Native importer exited {result.returncode} without a"
            f" valid response; evidence: {retained}",
        ) from error
    if status["code"]:
        raise StatusError(
            Code(status["code"]),
            f"{status['message']}; evidence: {retained}",
        )
    if result.returncode:
        raise StatusError(
            Code.INTERNAL,
            f"Importer exit {result.returncode}; evidence: {retained}",
        )
    return response


def probe_archive(resolution: Resolution, source: Path) -> dict:
    """Lists original native archive candidates without extracting firmware."""
    importer = resolution.settings.importer
    if importer is None or not importer.is_file():
        raise StatusError(
            Code.NOT_FOUND, f"Native importer unavailable: {importer}"
        )
    if not source.is_file():
        raise StatusError(Code.NOT_FOUND, str(source))
    cache = asset_directory("cache") / "firmware-imports"
    cache.mkdir(parents=True, exist_ok=True)
    retained = Path(tempfile.mkdtemp(prefix="probe-", dir=cache))
    response = native_request(
        [str(importer), "probe", str(source.resolve())], retained, retained, 60
    )
    return {**response["result"], "evidence": str(retained)}


def import_firmware(
    resolution: Resolution,
    *,
    source: Path,
    form: str = "archive",
    companion: Path | None = None,
    name: str | None = None,
    replace_alias: bool = False,
    variant: int = -1,
    timeout: float = 300,
) -> dict:
    """Runs the original native importer in retained staging, then seals it.

    Args:
        resolution: Tool/store selection with configuration origins.
        source: Archive, ROM, VPL, EKA2L1 instance, or exported bundle.
        form: Import form, never guessed from a phone model.
        companion: Matching RPKG or Z root for raw ROM imports.
        name: Optional local alias; it does not become part of the identity.
        replace_alias: Allows an existing alias to point at a different object.
        variant: Explicit VPL variant; -1 permits only a single variant.
        timeout: Bounded native process lifetime in seconds.
    """
    if name is not None and not re.fullmatch(
        r"[a-zA-Z0-9][a-zA-Z0-9_.-]{0,63}", name
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Alias must be 1–64 letters, digits, '.', '_' or '-'",
        )
    store = resolution.settings.store
    if store is None:
        raise StatusError(
            Code.FAILED_PRECONDITION, "No firmware store configured"
        )
    source = source.resolve()
    companion = companion.resolve() if companion else None
    if not source.exists():
        raise StatusError(Code.NOT_FOUND, str(source))
    cache = asset_directory("cache") / "firmware-imports"
    cache.mkdir(parents=True, exist_ok=True)
    retained = Path(tempfile.mkdtemp(prefix="import-", dir=cache))
    instance = retained / "instance"
    provenance = {"source": str(source), "form": form, "upstream": UPSTREAM}
    atomic_json(
        retained / "request.json",
        {**provenance, "resolution": resolution.model_dump(mode="json")},
    )
    try:
        if form == "bundle":
            original = validate_manifest(source)
            shutil.copytree(source / "instance", instance)
            device = original.device
            provenance["bundle_identity"] = original.identity
            provenance["original"] = original.provenance
        else:
            if form == "instance":
                candidates = list((source / "data/roms").glob("*/SYM.ROM"))
                if len(candidates) != 1:
                    raise StatusError(
                        Code.INVALID_ARGUMENT,
                        "Select a single ROM explicitly with --rom and"
                        " --z-drive; instance has multiple or no devices",
                    )
                source = candidates[0]
                companion = source.parents[2] / "drives/z" / source.parent.name
                form = "z"
            inputs = {str(source): digest(source)}
            if companion is not None:
                if not companion.exists():
                    raise StatusError(Code.NOT_FOUND, str(companion))
                inputs.update(
                    {
                        str(companion / key): value
                        for key, value in inventory(companion).items()
                    }
                    if companion.is_dir()
                    else {str(companion): digest(companion)}
                )
            if form == "vpl":
                # VPL references sibling FPSX/ROFS files. Retain all sibling
                # digests instead of claiming the VPL alone was consumed.
                inputs.update(
                    {
                        str(path): digest(path)
                        for path in source.parent.iterdir()
                        if path.is_file()
                    }
                )
            provenance["inputs"] = inputs
            importer = resolution.settings.importer
            if importer is None or not importer.is_file():
                raise StatusError(
                    Code.NOT_FOUND,
                    f"Native EKA2L1 importer unavailable: {importer}; configure"
                    " --importer or build symbian_firmware_tool",
                )
            provenance["importer_sha256"] = digest(importer)
            instance.mkdir()
            arguments = [
                str(importer),
                form,
                str(source),
                str(companion) if companion else "",
                str(variant),
            ]
            response = native_request(arguments, instance, retained, timeout)
            device = Device.model_validate(response["result"])
            if any(
                digest(Path(path)) != value for path, value in inputs.items()
            ):
                raise StatusError(
                    Code.DATA_LOSS, "Firmware source changed during import"
                )
        files = inventory(instance)
        manifest = Manifest(
            identity=identity(device, files),
            device=device,
            files=files,
            provenance=provenance,
        )
        atomic_json(
            retained / "firmware.json", manifest.model_dump(mode="json")
        )
        validate_manifest(retained)
        destination = store / "objects" / manifest.identity
        with locked(store):
            mapping = aliases(store)
            if (
                name in mapping
                and mapping[name] != manifest.identity
                and not replace_alias
            ):
                raise StatusError(
                    Code.ALREADY_EXISTS,
                    f"Alias {name!r} already selects {mapping[name]}; choose"
                    " another name or --replace-alias",
                )
            if destination.exists():
                validate_manifest(destination)
            else:
                destination.parent.mkdir(parents=True, exist_ok=True)
                publishing = Path(
                    tempfile.mkdtemp(prefix=".publish-", dir=destination.parent)
                )
                try:
                    shutil.copytree(instance, publishing / "instance")
                    atomic_json(
                        publishing / "firmware.json",
                        manifest.model_dump(mode="json"),
                    )
                    validate_manifest(publishing)
                    publishing.rename(destination)
                finally:
                    if publishing.exists():
                        shutil.rmtree(publishing)
            if name:
                mapping[name] = manifest.identity
                atomic_json(store / "aliases.json", mapping)
        result = {
            "firmware": f"sha256:{manifest.identity}",
            "alias": name,
            "directory": str(destination),
            "device": device.model_dump(),
            "evidence": str(retained),
        }
        atomic_json(retained / "result.json", result)
        # Successful import evidence keeps logs/manifests, not a second ROM/Z.
        shutil.rmtree(instance)
        return result
    except (StatusError, OSError, ValidationError) as error:
        atomic_json(retained / "failure.json", {"error": str(error)})
        if isinstance(error, ValidationError):
            raise StatusError(
                Code.DATA_LOSS,
                f"Invalid importer metadata; evidence: {retained}",
            ) from error
        raise


def list_firmware(store: Path) -> dict:
    """Lists catalog metadata without an expensive full-byte verification."""
    mapping = aliases(store)
    objects = []
    for directory in sorted((store / "objects").glob("*")):
        if directory.name.startswith("."):
            continue
        manifest = Manifest.model_validate_json(
            (directory / "firmware.json").read_text()
        )
        objects.append(
            {
                "firmware": f"sha256:{manifest.identity}",
                "aliases": sorted(
                    key
                    for key, value in mapping.items()
                    if value == manifest.identity
                ),
                "device": manifest.device.model_dump(),
                "directory": str(directory),
            }
        )
    return {
        "store": str(store),
        "objects": objects,
        "integrity_verified": False,
    }


def selected(resolution: Resolution) -> tuple[Path, Manifest]:
    """Verifies the explicitly selected baseline."""
    if resolution.settings.firmware is None:
        message = (
            "No firmware selected; import with 'symbian firmware import SOURCE"
            " --name NAME --use global', or pass --firmware sha256:ID"
        )
        raise StatusError(Code.FAILED_PRECONDITION, message)
    if resolution.settings.store is None:
        raise StatusError(
            Code.FAILED_PRECONDITION, "No firmware store configured"
        )
    directory = locate(resolution.settings.store, resolution.settings.firmware)
    manifest = validate_manifest(directory)
    if manifest.identity != directory.name:
        raise StatusError(
            Code.DATA_LOSS, "Firmware object directory/identity mismatch"
        )
    return directory / "instance", manifest


def svc_profile(manifest: Manifest, requested: str | None) -> str:
    """Enables a workaround only for its verified ROM and EUSER pair."""
    matches = (
        manifest.files.get(manifest.device.rom) == ROM_808
        and manifest.files.get(manifest.device.z_drive + "/sys/bin/euser.dll")
        == EUSER_808
    )
    if requested == "rm807-113.010.1508" and not matches:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            "rm807 SVC profile requires its exact verified ROM/EUSER pair; use"
            " auto or default",
        )
    return (
        "rm807-113.010.1508"
        if matches and requested != "default"
        else "default"
    )


def describe(resolution: Resolution) -> dict:
    """Explains selections, mappings and availability without launching."""
    result = resolution.model_dump(mode="json")
    result["host_tools"] = {
        key: {
            "path": str(path) if path else None,
            "available": path.is_file() if path else False,
        }
        for key, path in (
            ("emulator", resolution.settings.emulator),
            ("importer", resolution.settings.importer),
        )
    }
    try:
        instance, manifest = selected(resolution)
        result.update(
            firmware=f"sha256:{manifest.identity}",
            instance=str(instance),
            device=manifest.device.model_dump(),
            svc_profile=svc_profile(manifest, resolution.settings.profile),
            integrity_verified=True,
        )
        result["application_abi"] = (
            "supported EKA2 generation; device execution requires validation"
            if manifest.device.kernel == "eka2"
            else (
                "EKA1 no-UI ARMv5T process profile is available for the tested "
                "Nokia 7610 fixture (toolchain verify-eka1); GUI starters "
                "still require EKA2 startup/import ABI"
            )
        )
    except StatusError as error:
        result["selection_status"] = error.as_dict()
    return result


def export_firmware(store: Path, reference: str, destination: Path) -> dict:
    """Exports a verified bundle for offline transfer."""
    directory = locate(store, reference)
    manifest = validate_manifest(directory)
    if destination.exists():
        raise StatusError(Code.ALREADY_EXISTS, str(destination))
    shutil.copytree(directory, destination)
    validate_manifest(destination)
    return {
        "firmware": f"sha256:{manifest.identity}",
        "bundle": str(destination.resolve()),
    }
