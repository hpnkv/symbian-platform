#!/usr/bin/env python3
# Copyright 2026 The A11 Authors
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

"""Audit an installed SDK wheel, adapted from A11's distribution checks."""

import importlib
import importlib.metadata
import json
import re
import subprocess
import sys
import tempfile
import zipfile
from pathlib import Path

_LINUX_SYSTEM_LIBRARIES = re.compile(
    r"^(lib(c|m|pthread|rt|dl|util|resolv|stdc\+\+|gcc_s|z)\.so(?:\..*)?"
    r"|ld-linux[^/]*\.so(?:\..*)?)$"
)


def _output(*command: str) -> str:
    return subprocess.run(
        command, check=True, text=True, stdout=subprocess.PIPE
    ).stdout


def _macho_install_name(binary: Path) -> str | None:
    """A dylib's own ``LC_ID_DYLIB``, or None for anything that has none.

    ``otool -L`` lists this ahead of the real dependencies, so without it the
    bundled allocator fails its own audit: ``libmimalloc.2.1.dylib`` is built
    with an ``@rpath`` install name, which reads exactly like a dependency on
    itself.
    """
    if binary.suffix != ".dylib":
        return None
    lines = _output("otool", "-D", str(binary)).splitlines()
    return lines[1].strip() if len(lines) > 1 else None


def _audit_macos(binary: Path) -> None:
    install_name = _macho_install_name(binary)
    dependencies = _output("otool", "-L", str(binary)).splitlines()[1:]
    for line in dependencies:
        dependency = line.strip().split(" (", 1)[0]
        if dependency.startswith(("/usr/lib/", "/System/Library/")):
            continue
        if install_name is not None and dependency == install_name:
            continue
        raise RuntimeError(f"non-system Mach-O dependency: {dependency}")

    load_commands = _output("otool", "-l", str(binary)).splitlines()
    for index, line in enumerate(load_commands):
        if line.strip() == "cmd LC_RPATH":
            path = load_commands[index + 2].strip().split(" ", 1)[1]
            if not path.startswith(("@loader_path", "@executable_path")):
                raise RuntimeError(f"non-relocatable LC_RPATH: {path}")


def _audit_linux(binary: Path) -> None:
    dynamic = _output("readelf", "-d", str(binary))
    for dependency in re.findall(r"\(NEEDED\).*?\[(.*?)\]", dynamic):
        if not _LINUX_SYSTEM_LIBRARIES.match(dependency):
            raise RuntimeError(f"non-system ELF dependency: {dependency}")
    for value in re.findall(r"\((?:RPATH|RUNPATH)\).*?\[(.*?)\]", dynamic):
        for path in value.split(":"):
            if path and not path.startswith("$ORIGIN"):
                raise RuntimeError(f"non-relocatable ELF loader path: {path}")


def _audit_installed_behavior() -> None:
    """Check both native runtime modules, structured failures and resources."""
    from pydantic import BaseModel

    from symbian import _native
    from symbian.status import Status, StatusCode, StatusException

    for module in (
        "pybind11_abseil.status",
        "pybind11_abseil.ok_status_singleton",
    ):
        importlib.import_module(module)
    distribution = importlib.metadata.distribution("symbian-platform")
    for resource in (
        "symbian/toolchain/cmake/armv5t-pic.cmake",
        "symbian/toolchain/cmake/armv6-pic.cmake",
        "symbian/toolchain/cmake/symbian-arm.cmake",
        "symbian/toolchain/cmake/SymbianPic.cmake",
        "symbian/sdk/resources/header_probe.cc",
        "symbian/project/templates/app.cc",
        "symbian/project/templates/.clang-format",
        "symbian/project/templates/model.cc",
        "symbian/project/templates/icon.svg",
        "symbian/project/templates/SymbianApp.cmake",
        "symbian/project/templates/sdk.cmake",
        "symbian/project/templates/sdk-tool",
        "symbian/project/templates/python-bootstrap.py",
        "symbian/licenses/A11-LICENSE",
        "symbian/licenses/pybind11_abseil-LICENSE",
        "symbian/licenses/pybind11-LICENSE",
        "symbian/licenses/Abseil-LICENSE",
        "symbian/licenses/nlohmann-json-LICENSE",
    ):
        if not Path(distribution.locate_file(resource)).is_file():
            raise RuntimeError(f"Missing installed resource: {resource}")

    class Outcome(BaseModel):
        status: Status

    status = Status(
        code=StatusCode.NOT_FOUND,
        message="missing",
        details=[{"path": "absent"}],
    )
    if _native._status_roundtrip(status) != status:
        raise RuntimeError("Native status roundtrip lost payloads")
    if (
        Outcome.model_validate_json(
            Outcome(status=status).model_dump_json()
        ).status
        != status
    ):
        raise RuntimeError("Pydantic/native status roundtrip changed fields")
    try:
        _native.inspect_elf32(b"broken")
    except StatusException as error:
        if error.code != StatusCode.DATA_LOSS:
            raise RuntimeError("Native parser changed error code") from error
    else:
        raise RuntimeError("Native parser accepted a truncated artifact")
    result = subprocess.run(
        [sys.executable, "-m", "symbian.cli", "doctor", "--output-format=json"],
        check=True,
        capture_output=True,
        text=True,
    )
    if not json.loads(result.stdout)["result"]["native_analysis"]:
        raise RuntimeError("Installed CLI cannot find the native extension")


def main() -> None:
    """Check archive contents and execute the separately installed wheel."""
    if len(sys.argv) != 2:
        raise SystemExit("usage: python -m symbian.build_support.audit WHEEL")
    wheel = Path(sys.argv[1]).resolve()
    with tempfile.TemporaryDirectory() as temporary:
        root = Path(temporary)
        with zipfile.ZipFile(wheel) as archive:
            archive.extractall(root)
        binaries = [
            path
            for path in root.rglob("*")
            if path.suffix in {".so", ".dylib", ".pyd"}
        ]
        if len(binaries) != 3:
            raise RuntimeError(
                "Expected SDK extension and two Abseil runtime modules"
            )
        for binary in binaries:
            if sys.platform == "darwin":
                _audit_macos(binary)
            elif sys.platform.startswith("linux"):
                _audit_linux(binary)
            else:
                raise RuntimeError(f"Unsupported audit host: {sys.platform}")
    _audit_installed_behavior()
    print(f"Installed wheel audit passed: {wheel.name}")


if __name__ == "__main__":
    main()
