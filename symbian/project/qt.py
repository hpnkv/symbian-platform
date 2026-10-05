"""Exports original guest Qt headers and complete frozen library targets."""

import os
import shutil
import subprocess
from pathlib import Path

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
    shutil.move(str(output / "include"), sdk / "include/qt4")
    (sdk / "include/qt4/QtCore/qconfig.h").write_text(CONFIG)
    shutil.copyfile(
        source / "src/s60main/newallocator_hook.cpp",
        output / "newallocator_hook.cpp",
    )
    notices = sdk / "licenses/qt4"
    notices.mkdir(parents=True)
    for name in ("LICENSE.LGPL", "LGPL_EXCEPTION.txt", "LICENSE.FDL"):
        shutil.copyfile(source / name, notices / name)
    for library in ("QtCore", "QtGui"):
        build_import_proxy(
            source / f"src/s60installs/eabi/{library}u.def",
            [],
            library.lower() + ".dll",
            sdk / "proxies" / library.lower(),
            compiler,
            linker,
        )
