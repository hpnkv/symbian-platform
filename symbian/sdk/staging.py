"""Stages the GUI example's pinned source headers and native ordinal proxies."""

import hashlib
import json
import re
from pathlib import Path, PurePosixPath

from symbian.native import require_native
from symbian.sdk import build_import_proxy
from symbian.status import Code, StatusError


def _relative(value: str) -> PurePosixPath:
    if not isinstance(value, str) or not value or "\0" in value:
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid SDK relative path")
    path = PurePosixPath(value)
    if (
        not path.parts
        or path.is_absolute()
        or any(part in {".", "..", ""} for part in value.split("/"))
        or "\\" in value
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "SDK path must stay relative")
    return path


def _digest(path: Path, expected: str) -> bytes:
    if not isinstance(expected, str) or not re.fullmatch(
        r"[0-9a-f]{64}", expected
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "SDK SHA-256 is required")
    data = path.read_bytes()
    if hashlib.sha256(data).hexdigest() != expected:
        raise StatusError(Code.DATA_LOSS, f"SDK source digest mismatch: {path}")
    return data


def prepare_gui_sdk(profile: Path, sources_root: Path, output: Path) -> dict:
    """Stages explicit trusted headers and selected frozen function imports.

    Args:
        profile: Research source-selection manifest, separate from the app.
        sources_root: Directory containing the preserved upstream source trees.
        output: Separate disposable directory for header links and proxies.

    Returns:
        Header/proxy paths and digests. This source profile is not a matched
        Belle SDK or an emulator/device execution verdict.
    """
    profile, sources_root, output = (
        path.resolve() for path in (profile, sources_root, output)
    )
    if any(
        output.is_relative_to(root) or root.is_relative_to(output)
        for root in (profile.parent, sources_root)
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Keep SDK staging outside its input trees"
        )
    manifest_path = profile
    manifest_bytes = profile.read_bytes()
    try:
        manifest = json.loads(manifest_bytes)
        if manifest["schema"] != "symbian.gui-source-sdk/v1":
            raise ValueError("Unknown GUI SDK schema")
        headers = manifest["headers"]
        imports = manifest["imports"]
        repositories = manifest["repositories"]
        if not isinstance(headers, dict) or not 1 <= len(headers) <= 256:
            raise ValueError("Expected 1..256 headers")
        if not isinstance(imports, dict) or set(imports) != {
            "euser.dll",
            "ws32.dll",
        }:
            raise ValueError("Expected the GUI's two explicit DLL inputs")
        if (
            not isinstance(repositories, dict)
            or not repositories
            or any(
                not isinstance(name, str)
                or not re.fullmatch(r"[a-z][a-z0-9_]*", name)
                or not isinstance(revision, str)
                or not re.fullmatch(r"[0-9a-f]{40}", revision)
                for name, revision in repositories.items()
            )
        ):
            raise ValueError("Expected declared source repository revisions")
        for entry in headers.values():
            if not isinstance(entry, dict) or not {"source", "sha256"}.issubset(
                entry
            ):
                raise ValueError("Invalid header entry")
        for entry in imports.values():
            if not isinstance(entry, dict) or not {
                "definition",
                "definition_sha256",
                "symbols",
            }.issubset(entry):
                raise ValueError("Invalid import entry")
            symbols = entry["symbols"]
            if (
                not isinstance(symbols, list)
                or not 1 <= len(symbols) <= 256
                or any(not isinstance(symbol, str) for symbol in symbols)
            ):
                raise ValueError("Invalid import selection")
    except (ValueError, TypeError, KeyError) as error:
        raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error

    inputs = {manifest_path: manifest_bytes}

    def source(entry: dict, key: str, digest_key: str) -> Path:
        path = (sources_root / _relative(entry[key])).resolve()
        if not path.is_relative_to(sources_root):
            raise StatusError(Code.INVALID_ARGUMENT, "SDK input escaped root")
        inputs[path] = _digest(path, entry[digest_key])
        return path

    staged = {
        _relative(alias): source(entry, "source", "sha256")
        for alias, entry in headers.items()
    }
    definitions = {
        dll: source(entry, "definition", "definition_sha256")
        for dll, entry in imports.items()
    }
    # Validate both selections in the native core before staging either DLL.
    for dll, definition in definitions.items():
        require_native().generate_import_proxy(
            inputs[definition],
            imports[dll]["symbols"],
            dll.removesuffix(".dll") + ".dso",
            dll,
        )

    for directory in (output / "euser", output / "ws32"):
        if directory.is_symlink() or any(
            path.is_symlink() for path in directory.rglob("*")
        ):
            raise StatusError(
                Code.INVALID_ARGUMENT, "Proxy output contains a symlink"
            )
    if (output / "sdk-report.json").is_symlink():
        raise StatusError(Code.INVALID_ARGUMENT, "SDK report is a symlink")

    include = output / "include"
    if include.is_symlink():
        raise StatusError(
            Code.INVALID_ARGUMENT, "SDK include root is a symlink"
        )
    include.mkdir(parents=True, exist_ok=True)
    # Check every destination before publishing links. Existing source links can
    # be reused; regular files and redirected directories are not overwritten.
    for alias, original in staged.items():
        destination = include / alias
        if not destination.parent.resolve().is_relative_to(include):
            raise StatusError(Code.INVALID_ARGUMENT, "SDK link escaped include")
        if destination.is_symlink():
            if destination.resolve() != original:
                raise StatusError(
                    Code.ALREADY_EXISTS, "Different SDK link exists"
                )
        elif destination.exists():
            raise StatusError(
                Code.ALREADY_EXISTS, "SDK header path is occupied"
            )
    for alias, original in staged.items():
        destination = include / alias
        destination.parent.mkdir(parents=True, exist_ok=True)
        if not destination.is_symlink():
            destination.symlink_to(original)
    proxies = {
        dll: build_import_proxy(
            definition,
            imports[dll]["symbols"],
            dll,
            output / dll.removesuffix(".dll"),
        )
        for dll, definition in definitions.items()
    }
    if any(path.read_bytes() != data for path, data in inputs.items()):
        raise StatusError(Code.ABORTED, "SDK staging inputs changed")
    report = {
        "schema": "symbian.gui-source-sdk-preparation/v1",
        "include": str(include),
        "header_count": len(staged),
        "repositories": repositories,
        "repository_revisions_verified": False,
        "inputs": {
            str(path): hashlib.sha256(data).hexdigest()
            for path, data in sorted(inputs.items())
        },
        "proxies": proxies,
        "sdk_verified": False,
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "limitations": [
            "Pinned public source profile; "
            "actual Belle exports remain unverified",
            "Header links depend on retained original source trees",
            "No target implementations, resources, ROM or Z drive are staged",
        ],
    }
    (output / "sdk-report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report
