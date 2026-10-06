"""Stages original Qt Mobility 1.0.3 public headers and frozen imports."""

import hashlib
import json
import shutil
from pathlib import Path

from symbian.native import require_native
from symbian.sdk import build_import_proxy


def _checked_copy(source: Path, destination: Path, expected: str) -> None:
    """Copies one pinned source file after verifying its recorded digest."""
    contents = source.read_bytes()
    if hashlib.sha256(contents).hexdigest() != expected:
        raise ValueError(f"Qt Mobility source changed: {source}")
    destination.parent.mkdir(parents=True, exist_ok=True)
    destination.write_bytes(contents)


def prepare_qt_mobility(
    workspace: Path, sdk: Path, compiler: str, linker: str
) -> None:
    """Publishes only qmake PUBLIC_HEADERS and their original class aliases."""
    source = workspace / "research/upstream/qtmobility"
    manifest = workspace / "research/native-sdk/qt-mobility.json"
    data = json.loads(manifest.read_text())
    output = sdk / "share/symbian/qtmobility"
    output.mkdir(parents=True, exist_ok=True)
    global_header = data["global_header"]
    _checked_copy(
        source / global_header["source"],
        sdk / "include/qtmobility" / global_header["include"],
        global_header["sha256"],
    )
    for module in data["modules"]:
        for header in module["headers"]:
            original = source / header["source"]
            destination = sdk / "include/qtmobility" / header["include"]
            _checked_copy(original, destination, header["sha256"])
        for alias in module["aliases"]:
            (sdk / "include/qtmobility" / alias["include"]).write_text(
                f'#include "{alias["header"]}"\n'
            )
        definition = source / module["definition"]
        frozen = output / "defs" / definition.name
        _checked_copy(definition, frozen, module["definition_sha256"])
        dll = module["dll"].removesuffix(".dll")
        build_import_proxy(
            frozen,
            [],
            module["dll"],
            sdk / "proxies" / dll,
            compiler,
            linker,
        )
    notices = sdk / "licenses/qtmobility"
    notices.mkdir(parents=True, exist_ok=True)
    for name in data["licenses"]:
        shutil.copyfile(source / name, notices / name)
    shutil.copyfile(manifest, output / "inventory.json")
    validate_qt_mobility(sdk)


def validate_qt_mobility(sdk: Path) -> None:
    """Checks every delivered public header and complete frozen interface."""
    data = json.loads(
        (sdk / "share/symbian/qtmobility/inventory.json").read_text()
    )
    native = require_native()
    records = [data["global_header"]] + [
        header for module in data["modules"] for header in module["headers"]
    ]
    for record in records:
        header = sdk / "include/qtmobility" / record["include"]
        if (
            not header.is_file()
            or hashlib.sha256(header.read_bytes()).hexdigest()
            != record["sha256"]
        ):
            raise ValueError(f"Missing or altered Qt Mobility header: {header}")
    for module in data["modules"]:
        for alias in module["aliases"]:
            path = sdk / "include/qtmobility" / alias["include"]
            if not path.is_file() or path.read_text() != (
                f'#include "{alias["header"]}"\n'
            ):
                raise ValueError(
                    f"Missing or altered Qt Mobility alias: {path}"
                )
        frozen = (
            sdk
            / "share/symbian/qtmobility/defs"
            / Path(module["definition"]).name
        )
        if (
            not frozen.is_file()
            or hashlib.sha256(frozen.read_bytes()).hexdigest()
            != module["definition_sha256"]
        ):
            raise ValueError(f"Missing Qt Mobility frozen definition: {frozen}")
        dll = module["dll"].removesuffix(".dll")
        proxy = sdk / "proxies" / dll / f"{dll}.dso"
        if not proxy.is_file():
            raise ValueError(f"Missing Qt Mobility import interface: {proxy}")
        expected = {
            (entry.symbol, entry.ordinal, entry.data)
            for entry in native.parse_def(frozen.read_bytes())
            if not entry.absent
        }
        actual = native.inspect_import_proxy(proxy.read_bytes())
        if (
            actual.target_dll != module["dll"]
            or {
                (entry.symbol, entry.ordinal, entry.data)
                for entry in actual.exports
            }
            != expected
        ):
            raise ValueError(f"Qt Mobility import mismatch: {proxy}")
