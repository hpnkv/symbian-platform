"""Original Open C header layout and known source exceptions."""

import json
import os
import subprocess
from pathlib import Path

import pytest

from symbian.project.sdk import AppSdk

SDK = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(not SDK, reason="Set SYMBIAN_APP_SDK")


def test_public_open_c_headers_compile_or_report_blocker(tmp_path):
    """Compiles each delivered libc header in its declared language profile."""
    sdk = AppSdk.load(Path(SDK))
    native = sdk.prefix / "share/symbian/native"
    inventory = json.loads((native / "inventory.json").read_text())
    usage = json.loads((native / "openc-header-usage.json").read_text())
    facility = next(
        item for item in inventory["facilities"] if item["target"] == "OpenC"
    )
    headers = facility["headers"]
    assert len(headers) >= 108
    assert set(usage["profiles"]) <= set(headers)
    assert usage["profiles"]["stdapis/sys/event.h"]["blocked"]
    cmake = [
        "cmake_minimum_required(VERSION 3.28)",
        "project(openc_header_canaries LANGUAGES C CXX ASM)",
        "include(SymbianApp)",
        "add_custom_target(openc_header_canaries ALL)",
    ]
    compiled = 0
    for index, header in enumerate(headers):
        profile = usage["profiles"].get(header, {})
        if profile.get("blocked"):
            continue
        extension = "cc" if profile.get("language") == "C++" else "c"
        source = tmp_path / f"canary_{index}.{extension}"
        spelling = header.removeprefix("stdapis/")
        includes = profile.get("preinclude", []) + [
            profile.get("include_via", spelling)
        ]
        body = "".join(f"#include <{name}>\n" for name in includes)
        if profile.get("indirect_guard"):
            body += (
                f"#ifndef {profile['indirect_guard']}\n"
                '#error "Expected original indirect header"\n'
                "#endif\n"
            )
        source.write_text(body)
        cmake.extend(
            [
                f"add_library(canary_{index} OBJECT "
                f"canary_{index}.{extension})",
                f"target_link_libraries(canary_{index} PRIVATE "
                "Symbian::Runtime Symbian::OpenC)",
                f"add_dependencies(openc_header_canaries canary_{index})",
            ]
        )
        compiled += 1
    assert compiled == len(headers) - 1
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
