"""Exports original guest Qt headers and complete frozen library targets."""

import hashlib
import json
import os
import shutil
import subprocess
from pathlib import Path

from symbian.native import require_native
from symbian.sdk import build_import_proxy

CONFIG = """#pragma once
#define QT_ARCH_SYMBIAN
#define QT_POINTER_SIZE 4
#define Q_BYTE_ORDER Q_LITTLE_ENDIAN
#define QT_SHARED
#define QT_NO_DEBUG
#define QT_EDITION QT_EDITION_OPENSOURCE
#define QT_NO_STL
"""

MODULES = (
    "QtCore",
    "QtGui",
    "QtNetwork",
    "QtSql",
    "QtXml",
    "QtOpenGL",
    "QtSvg",
    "QtScript",
    "QtXmlPatterns",
    "QtDeclarative",
    "QtMultimedia",
    "QtOpenVG",
    "QtTest",
    "QtWebKit",
)


def prepare_qt(source: Path, sdk: Path, compiler: str, linker: str) -> None:
    """Stages Qt 4.8.1 once in an SDK, independently of consumer sources.

    Args:
        source: Pinned, unconfigured original Qt source directory.
        sdk: Target SDK output directory.
        compiler: ARM-capable Clang executable.
        linker: ELF LLD executable.
    """
    source, sdk = source.resolve(), sdk.resolve()
    if (
        "#define QT_VERSION 0x040801"
        not in (source / "src/corelib/global/qglobal.h").read_text()
    ):
        raise ValueError("Guest Qt SDK requires Qt 4.8.1 sources")
    output = sdk / "share/symbian/qt"
    output.mkdir(parents=True)
    inventory = json.loads(
        (source.parents[2] / "research/native-sdk/qt-modules.json").read_text()
    )
    subprocess.run(
        [
            "perl",
            str(source / "bin/syncqt"),
            "-outdir",
            str(output),
            "-copy",
            "-quiet",
            *(
                argument
                for module in MODULES
                for argument in ("-module", module)
            ),
        ],
        env={**os.environ, "QTDIR": str(source)},
        check=True,
    )
    shutil.move(str(output / "include"), sdk / "include/qt4")
    for module in MODULES:
        shutil.rmtree(
            sdk / "include/qt4" / module / "private", ignore_errors=True
        )
    for pattern in ("*_p.h", "*_qpa.h"):
        for private_header in (sdk / "include/qt4").rglob(pattern):
            private_header.unlink()
    (sdk / "include/qt4/QtCore/qconfig.h").write_text(CONFIG)
    shutil.copyfile(
        source / "src/s60main/newallocator_hook.cpp",
        output / "newallocator_hook.cpp",
    )
    notices = sdk / "licenses/qt4"
    notices.mkdir(parents=True)
    for name in ("LICENSE.LGPL", "LGPL_EXCEPTION.txt", "LICENSE.FDL"):
        shutil.copyfile(source / name, notices / name)
    for library in MODULES:
        definition = (
            source
            / "src/3rdparty/webkit/Source/WebKit/qt/symbian/eabi/QtWebKitu.def"
            if library == "QtWebKit"
            else source / f"src/s60installs/eabi/{library}u.def"
        )
        record = next(
            item
            for item in inventory["modules"]
            if item["target"] == f"Symbian::{library}"
        )
        if (
            hashlib.sha256(definition.read_bytes()).hexdigest()
            != record["definition_sha256"]
        ):
            raise ValueError(f"Original {library} frozen DEF changed")
        frozen = output / "defs" / definition.name
        frozen.parent.mkdir(exist_ok=True)
        shutil.copyfile(definition, frozen)
        build_import_proxy(
            definition,
            [],
            library.lower() + ".dll",
            sdk / "proxies" / library.lower(),
            compiler,
            linker,
        )
    shutil.copyfile(
        source.parents[2] / "research/native-sdk/qt-modules.json",
        output / "modules.json",
    )
    validate_qt_payload(sdk)


def validate_qt_payload(sdk: Path) -> None:
    """Checks original public headers and frozen ordinals after relocation."""
    data = json.loads((sdk / "share/symbian/qt/modules.json").read_text())
    native = require_native()
    for header in (sdk / "include/qt4").rglob("*"):
        if header.is_file() and (
            "private" in header.parts
            or header.name.endswith(("_p.h", "_qpa.h"))
        ):
            raise ValueError(f"Private Qt header exposed in SDK: {header}")
    for module in data["modules"]:
        for relative, digest in module["headers"].items():
            header = sdk / "include/qt4" / relative
            if (
                not header.is_file()
                or hashlib.sha256(header.read_bytes()).hexdigest() != digest
            ):
                raise ValueError(
                    f"Missing or altered Qt public header: {header}"
                )
        name = Path(module["source_definition"]).name
        definition = sdk / "share/symbian/qt/defs" / name
        if (
            not definition.is_file()
            or hashlib.sha256(definition.read_bytes()).hexdigest()
            != module["definition_sha256"]
        ):
            raise ValueError(f"Missing or altered Qt frozen DEF: {definition}")
        dll = module["dll"].removesuffix(".dll")
        proxy = sdk / "proxies" / dll / f"{dll}.dso"
        if not proxy.is_file():
            raise ValueError(f"Missing Qt import interface: {proxy}")
        expected = {
            (entry.symbol, entry.ordinal, entry.data)
            for entry in native.parse_def(definition.read_bytes())
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
            raise ValueError(f"Qt frozen import interface mismatch: {proxy}")
