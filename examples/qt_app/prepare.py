"""Prepares original Qt 4.8.1 headers and selected guest DLL imports."""

import argparse
import json
import os
import shutil
import subprocess
import urllib.request
from pathlib import Path

from symbian.project.sdk import AppSdk, discover_sdk
from symbian.sdk import build_import_proxy, inspect_proxy

QT_SYMBOLS = (
    "_Z26qt_symbian_SetupThreadHeapiR24SStdEpocThreadCreateInfo",
    "_ZN11QPushButtonC1ERK7QStringP7QWidget",
    "_ZN11QPushButtonD1Ev",
    "_ZN12QApplication17setGraphicsSystemERK7QString",
    "_ZN12QApplication4execEv",
    "_ZN12QApplicationC1ERiPPci",
    "_ZN12QApplicationD1Ev",
    "_ZN16QCoreApplication12setAttributeEN2Qt20ApplicationAttributeEb",
    "_ZN7QObject10disconnectEPKS_PKcS1_S3_",
    "_ZN7QObject7connectEPKS_PKcS1_S3_N2Qt14ConnectionTypeE",
    "_ZN7QString4freeEPNS_4DataE",
    "_ZN7QString8fromUtf8EPKci",
    "_ZN7QWidget14setWindowTitleERK7QString",
    "_ZN7QWidget14showFullScreenEv",
    "_ZN7QWidget6resizeERK5QSize",
)
KERNEL_DEF = (
    "https://raw.githubusercontent.com/SymbianSource/"
    "oss.FCL.sf.os.kernelhwsrv/"
    "0c3208650587ac0230aed8a74e9bddb5288023eb/"
    "kernel/eka/eabi/euseru.def"
)
CONFIG = """#pragma once
#define QT_ARCH_SYMBIAN
#define QT_POINTER_SIZE 4
#define Q_BYTE_ORDER Q_LITTLE_ENDIAN
#define QT_SHARED
#define QT_NO_DEBUG
#define QT_EDITION QT_EDITION_OPENSOURCE
#define QT_NO_STL
"""


def prepare(source: Path, sdk: AppSdk, kernel_def: Path | None) -> None:
    """Stages guest headers, allocator hook and frozen native import proxies.

    Args:
        source: Unconfigured original Qt 4.8.1 source checkout.
        sdk: Installed ARM SDK supplying Clang, LLD and original OS headers.
        kernel_def: Optional original EUSER EABI DEF for offline preparation.
    """
    from symbian.native import require_native

    source = source.resolve()
    version = (source / "src/corelib/global/qglobal.h").read_text()
    if "#define QT_VERSION 0x040801" not in version:
        raise ValueError("This example requires the original Qt 4.8.1 headers")
    project = Path(__file__).resolve().parent
    output = project / ".symbian/qt"
    output.mkdir(parents=True, exist_ok=True)
    # The wheel's proxy toolchain also finds ld.lld by name during configure.
    # Resolve it from the installed SDK rather than a system LLVM installation.
    os.environ["PATH"] = os.pathsep.join(
        (
            str(sdk.compiler.parent),
            str(sdk.linker.parent),
            os.environ.get("PATH", ""),
        )
    )
    subprocess.run(
        [
            "perl",
            str(source / "bin/syncqt"),
            "-outdir",
            str(output),
            "-copy",
            "-quiet",
            "-module",
            "QtCore",
            "-module",
            "QtGui",
        ],
        env={**os.environ, "QTDIR": str(source)},
        check=True,
    )
    (output / "include/QtCore/qconfig.h").write_text(CONFIG)
    shutil.copyfile(
        source / "src/s60main/newallocator_hook.cpp",
        output / "newallocator_hook.cpp",
    )
    licenses = output / "licenses"
    licenses.mkdir(exist_ok=True)
    for name in ("LICENSE.LGPL", "LGPL_EXCEPTION.txt", "LICENSE.FDL"):
        shutil.copyfile(source / name, licenses / name)

    native = require_native()
    remaining = set(QT_SYMBOLS)
    for library in ("QtCore", "QtGui"):
        definition = source / f"src/s60installs/eabi/{library}u.def"
        exports = native.parse_def(definition.read_bytes())
        selected = [item.symbol for item in exports if item.symbol in remaining]
        build_import_proxy(
            definition,
            selected,
            library.lower() + ".dll",
            output / "proxies" / library.lower(),
            compiler=str(sdk.compiler),
            linker=str(sdk.linker),
        )
        remaining.difference_update(selected)
    if remaining:
        raise ValueError(f"Qt export definitions lack {sorted(remaining)}")

    if kernel_def is None:
        kernel_def = output / "euseru.def"
        if not kernel_def.exists():
            with urllib.request.urlopen(KERNEL_DEF, timeout=60) as response:
                kernel_def.write_bytes(response.read())
    symbols = [
        item["symbol"]
        for item in inspect_proxy(sdk.prefix / "proxies/euser/euser.dso")[
            "exports"
        ]
    ]
    # Qt's inline QString reference counting calls the original EUSER export.
    symbols.append("_ZN4User9LockedDecERi")
    build_import_proxy(
        kernel_def,
        list(dict.fromkeys(symbols)),
        "euser.dll",
        output / "proxies/euser",
        compiler=str(sdk.compiler),
        linker=str(sdk.linker),
    )
    (project / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)}) + "\n"
    )


def main() -> None:
    """Prepares the example using an installed SDK and original Qt sources."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--qt-source", required=True, type=Path)
    parser.add_argument("--sdk", type=Path)
    parser.add_argument("--kernel-def", type=Path)
    args = parser.parse_args()
    prepare(
        args.qt_source,
        AppSdk.load(discover_sdk(args.sdk)),
        args.kernel_def,
    )


if __name__ == "__main__":
    main()
