"""Host policy for the canonical SISX application package."""

import hashlib
import json
import tomllib
from pathlib import Path

from symbian.native import require_native
from symbian.project.libraries import package_libraries
from symbian.packaging.ca_bundle import digest as ca_digest
from symbian.packaging.ca_bundle import selected_bundle
from symbian.packaging.registration import compile_registration
from symbian.status import Code, StatusError

_MAX_PACKAGE = 16 * 1024 * 1024 + 64 * 1024


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
            for field in (
                "executable_uid",
                "executable_size",
                "executable_sha1",
                "target",
            )
        }
    )
    result["application_registered"] = info.application_registered
    result["signed_package"] = info.signed_package
    result["files"] = [
        {
            field: getattr(file, field)
            for field in ("target", "size", "sha1", "capabilities")
        }
        for file in info.files
    ]
    return result


def package(
    project: Path,
    artifact: Path,
    output: Path,
    *,
    signing_certificate: Path | None = None,
    signing_key: Path | None = None,
) -> dict:
    """Packages one E32 application using the project's package table.

    Args:
        project: Directory containing symbian.toml with a package table.
        artifact: Native converter's validated E32 application executable.
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
        manifest_data = tomllib.loads(manifest_bytes.decode("utf-8"))
        options = manifest_data["package"]
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
    application = manifest_data.get("application")
    resources = (
        compile_registration(
            project,
            application,
            options["executable_name"],
            native.inspect_e32(data).uid3,
        )
        if application is not None
        else None
    )

    assets, asset_hashes = resources if resources is not None else (None, {})
    libraries = package_libraries(artifact)
    ca_bundle = selected_bundle(project, artifact)
    if ca_bundle is not None:
        if assets is None:
            raise StatusError(
                Code.INVALID_ARGUMENT,
                "CA bundle requires an [application] registration",
            )
        ca_path, ca_data = ca_bundle
        stem = Path(options["executable_name"]).stem
        target = f"!:\\resource\\apps\\{stem}_ca.pem"
        insertion = len(assets) - int(assets[-1][0].endswith(".mif"))
        assets.insert(insertion, (target, ca_data))
        asset_hashes[str(ca_path.relative_to(project))] = ca_digest(ca_data)

    def build() -> bytes:
        if resources is None and not libraries:
            return native.build_sis(data, **options)
        return native.build_application_sis(
            data, assets or [], **options, libraries=libraries
        )

    if (signing_certificate is None) != (signing_key is None):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Both signing certificate and key are required",
        )
    certificate = (
        _read_bounded(signing_certificate, 16 * 1024)
        if signing_certificate is not None
        else None
    )
    private_key = (
        _read_bounded(signing_key, 16 * 1024)
        if signing_key is not None
        else None
    )

    def build_final() -> bytes:
        unsigned = build()
        if certificate is None or private_key is None:
            return unsigned
        return native.sign_sis(unsigned, certificate, private_key)

    first = build_final()
    if first != build_final():
        raise StatusError(Code.DATA_LOSS, "Repeated SIS generation differs")
    if manifest.read_bytes() != manifest_bytes or artifact.read_bytes() != data:
        raise StatusError(Code.ABORTED, "Package inputs changed")
    if package_libraries(artifact) != libraries:
        raise StatusError(Code.ABORTED, "Application DLL inputs changed")
    for relative_path, digest in asset_hashes.items():
        source = (project / relative_path).resolve()
        if not source.is_relative_to(project) or not source.is_file():
            raise StatusError(
                Code.ABORTED, "Application asset moved while packaging"
            )
        with source.open("rb") as stream:
            current = stream.read(1024 * 1024 + 1)
        if hashlib.sha256(current).hexdigest() != digest:
            raise StatusError(
                Code.ABORTED, "Application asset changed while packaging"
            )
    if signing_certificate is not None and signing_key is not None:
        if (
            _read_bounded(signing_certificate, 16 * 1024) != certificate
            or _read_bounded(signing_key, 16 * 1024) != private_key
        ):
            raise StatusError(Code.ABORTED, "Signing identity changed")
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
        "application_asset_sha256": asset_hashes,
        "ca_bundle": (
            {
                "source": str(ca_bundle[0]),
                "target": (
                    "!:\\resource\\apps\\"
                    f"{Path(options['executable_name']).stem}_ca.pem"
                ),
                "sha256": ca_digest(ca_bundle[1]),
            }
            if ca_bundle is not None
            else None
        ),
        "sis": inspect_package(path),
        "reproducible": True,
        "reproducibility_scope": "two native writer calls on this host",
        "unsigned": certificate is None,
        "signed": certificate is not None,
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "limitations": [
            "SVG-in-MIF icon rendering on physical Belle is not yet verified",
            "No SIS scripts or package dependencies",
            "Imported DLL implementations must already exist in the target",
            "Fixed 2004-01-01 timestamp, uncompressed streams, ASCII metadata",
            "SHA-1 is legacy integrity; signing does not establish phone trust",
            "Phone installation and matched Belle runtime remain unverified",
        ],
    }
    report_path.write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report


__all__ = ["inspect_package", "package"]
