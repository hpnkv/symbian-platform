"""Independent public Qt Mobility header compilation against an SDK."""

import json
import os
import subprocess
from pathlib import Path

import pytest

from symbian.project.sdk import AppSdk

SDK = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(not SDK, reason="Set SYMBIAN_APP_SDK")


def test_public_qt_mobility_headers_compile_independently(tmp_path):
    """Each declared header compiles with its owning target's dependencies."""
    sdk = AppSdk.load(Path(SDK))
    inventory = json.loads(
        (sdk.prefix / "share/symbian/qtmobility/inventory.json").read_text()
    )
    cmake = [
        "cmake_minimum_required(VERSION 3.28)",
        "project(qtmobility_header_canaries LANGUAGES CXX ASM)",
        "include(SymbianApp)",
        "add_custom_target(qtmobility_header_canaries ALL)",
    ]
    count = 0
    for module in inventory["modules"]:
        for header in module["headers"]:
            source = tmp_path / f"canary_{count}.cc"
            includes = header.get("preinclude", []) + [header["include"]]
            source.write_text(
                "".join(f"#include <{item}>\n" for item in includes)
            )
            cmake.extend(
                [
                    f"add_library(canary_{count} OBJECT canary_{count}.cc)",
                    f"target_link_libraries(canary_{count} PRIVATE "
                    f"Symbian::Runtime {module['target']})",
                    "add_dependencies(qtmobility_header_canaries "
                    f"canary_{count})",
                ]
            )
            count += 1
    assert count >= 180
    (tmp_path / "CMakeLists.txt").write_text("\n".join(cmake) + "\n")
    build = tmp_path / "build"
    configure = subprocess.run(
        [
            "cmake",
            "-G",
            "Ninja",
            "-S",
            str(tmp_path),
            "-B",
            str(build),
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix / 'cmake/symbian-arm.cmake'}",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            "-DSYMBIAN_TARGET_ARCH=armv6",
        ],
        capture_output=True,
        text=True,
    )
    assert configure.returncode == 0, configure.stdout + configure.stderr
    result = subprocess.run(
        ["cmake", "--build", str(build), "-j", "6"],
        capture_output=True,
        text=True,
    )
    assert result.returncode == 0, result.stdout + result.stderr
