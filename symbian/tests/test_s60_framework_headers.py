"""Independent application-framework and utility public-header checks."""

import json
import os
import subprocess
from pathlib import Path

import pytest

from symbian.project.sdk import AppSdk

SDK = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(not SDK, reason="Set SYMBIAN_APP_SDK")


def test_public_s60_framework_headers_compile_independently(tmp_path):
    """Checks original application headers with only their owner targets."""
    sdk = AppSdk.load(Path(SDK))
    inventory = json.loads(
        (sdk.prefix / "share/symbian/native/inventory.json").read_text()
    )
    cmake = [
        "cmake_minimum_required(VERSION 3.28)",
        "project(s60_header_canaries LANGUAGES CXX ASM)",
        "include(SymbianApp)",
        "add_custom_target(s60_header_canaries ALL)",
    ]
    count = 0
    for name in (
        "AppArc",
        "Eikon",
        "Cone",
        "CentralRepository",
        "CenRepNotification",
        "Bafl",
        "StreamsNative",
        "Calendar",
        "Messaging",
        "Uri",
        "HttpNative",
        "Mime",
    ):
        facility = next(
            item for item in inventory["facilities"] if item["target"] == name
        )
        assert not facility.get("blocked")
        for header in facility["headers"]:
            (tmp_path / f"canary_{count}.cc").write_text(
                f"#include <{header}>\n"
            )
            cmake.extend(
                [
                    f"add_library(canary_{count} OBJECT canary_{count}.cc)",
                    f"target_link_libraries(canary_{count} PRIVATE "
                    f"Symbian::Runtime Symbian::{name})",
                    f"add_dependencies(s60_header_canaries canary_{count})",
                ]
            )
            count += 1
    assert count == 51
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
