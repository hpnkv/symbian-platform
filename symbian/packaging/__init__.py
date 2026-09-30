"""Host policy for the canonical unsigned SISX experiment."""

import hashlib
import json
import tomllib
from pathlib import Path

from symbian.native import require_native
from symbian.status import Code, StatusError

_MAX_PACKAGE = 16 * 1024 * 1024 + 4096


def _read_bounded(path: Path, limit: int) -> bytes:
    with path.open("rb") as stream:
        data = stream.read(limit + 1)
    if len(data) > limit:
        raise StatusError(Code.RESOURCE_EXHAUSTED, "SIS input exceeds limit")
    return data


def inspect_package(path: Path) -> dict:
    """Returns metadata after native checksum, digest and profile checks."""
    info = require_native().inspect_sis(_read_bounded(path, _MAX_PACKAGE))
    result = {
        field: getattr(info.options, field)
        for field in ("uid", "name", "vendor", "executable_name", "version")
    }
    result.update(
        {
            field: getattr(info, field)
            for field in ("executable_uid", "executable_size", "target")
        }
    )
    return result


def package(project: Path, artifact: Path, output: Path) -> dict:
    """Packages one experimental E32 using the project's package table.

    Args:
        project: Directory containing symbian.toml with a package table.
        artifact: Native converter's import-free experimental E32 executable.
        output: Directory for a SIS file and a structured evidence report.

    Returns:
        Package paths, digests and profile metadata without a runtime verdict.
    """
    project, artifact, output = (
        path.resolve() for path in (project, artifact, output)
    )
    manifest = project / "symbian.toml"
    manifest_bytes = manifest.read_bytes()
    try:
        options = tomllib.loads(manifest_bytes.decode("utf-8"))["package"]
    except (UnicodeError, tomllib.TOMLDecodeError, KeyError) as error:
        raise StatusError(
            Code.INVALID_ARGUMENT, "Expected a package table in symbian.toml"
        ) from error
    fields = {"uid", "name", "vendor", "executable_name", "version"}
    if not isinstance(options, dict) or set(options) != fields:
        raise StatusError(
            Code.INVALID_ARGUMENT, "Package table requires exactly five fields"
        )
    if (
        type(options["uid"]) is not int
        or not 0 <= options["uid"] <= 0xFFFFFFFF
        or any(
            not isinstance(options[field], str)
            for field in ("name", "vendor", "executable_name")
        )
        or not isinstance(options["version"], list)
        or len(options["version"]) != 3
        or any(
            type(part) is not int or not -(2**31) <= part < 2**31
            for part in options["version"]
        )
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid package field types")
    native = require_native()
    data = _read_bounded(artifact, 16 * 1024 * 1024)
    first = native.build_sis(data, **options)
    if first != native.build_sis(data, **options):
        raise StatusError(Code.DATA_LOSS, "Repeated SIS generation differs")
    if manifest.read_bytes() != manifest_bytes or artifact.read_bytes() != data:
        raise StatusError(Code.ABORTED, "Package inputs changed")
    path = output / f"{Path(options['executable_name']).stem}.sis"
    report_path = output / "package-report.json"
    if path.resolve() in (artifact, manifest) or report_path.resolve() in (
        artifact,
        manifest,
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Package would overwrite input"
        )
    output.mkdir(parents=True, exist_ok=True)
    path.write_bytes(first)
    report = {
        "schema": "symbian.sis-experiment/v1",
        "artifact": str(path),
        "sha256": hashlib.sha256(first).hexdigest(),
        "input": str(artifact),
        "input_sha256": hashlib.sha256(data).hexdigest(),
        "manifest": str(manifest),
        "manifest_sha256": hashlib.sha256(manifest_bytes).hexdigest(),
        "sis": inspect_package(path),
        "reproducible": True,
        "reproducibility_scope": "two native writer calls on this host",
        "unsigned": True,
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "limitations": [
            "One English import-free EXE; no resources/scripts/dependencies",
            "Fixed 2004-01-01 timestamp, uncompressed streams, ASCII metadata",
            "SHA-1 is legacy file integrity; no certificate or signing policy",
            "Phone installation and matched Belle runtime remain unverified",
        ],
    }
    report_path.write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report


__all__ = ["inspect_package", "package"]
