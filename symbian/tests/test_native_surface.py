"""Configuration and installed-payload controls for public native APIs."""

import hashlib
import json
import subprocess
from pathlib import Path

import pytest

from symbian.project.native_surface import validate_native_payload
from symbian.project.qt import validate_qt_payload

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture
def configure(tmp_path):
    sdk = tmp_path / "relocated SDK"
    entries = [
        {
            "target": "Audio",
            "dll": "audio.dll",
            "headers": ["audio.h"],
            "dependencies": ["Common"],
            "definition": {
                "destination": "share/symbian/native/defs/audio.def"
            },
        },
        {
            "target": "Common",
            "dll": "common.dll",
            "headers": ["common.h"],
            "dependencies": [],
            "definition": {
                "destination": "share/symbian/native/defs/common.def"
            },
        },
        {
            "target": "Blocked",
            "dll": "blocked.dll",
            "headers": ["blocked.h"],
            "dependencies": [],
            "blocked": "Missing frozen EABI evidence",
        },
    ]
    for entry in entries[:2]:
        dll = entry["dll"].removesuffix(".dll")
        for relative in [
            f"proxies/{dll}/{dll}.dso",
            f"include/native/{dll}.h",
            entry["definition"]["destination"],
        ]:
            p = sdk / relative
            p.parent.mkdir(parents=True, exist_ok=True)
            p.write_text("configuration fixture")
    metadata = sdk / "share/symbian/native/inventory.json"
    metadata.write_text(json.dumps({"facilities": entries}))

    def run(body, extra=""):
        (tmp_path / "app.cc").write_text("int main() { return 0; }\n")
        (tmp_path / "CMakeLists.txt").write_text(
            "cmake_minimum_required(VERSION 3.28)\n"
            "project(check LANGUAGES CXX)\n"
            "set(CMAKE_SYSTEM_NAME Generic)\n"
            "set(SYMBIAN_TARGET_ARCH armv6)\n"
            "set(CMAKE_CXX_COMPILER_TARGET armv6-none-eabi)\n"
            f'set(SYMBIAN_SDK_PREFIX "{sdk}")\n'
            "add_library(Symbian::EUser INTERFACE IMPORTED GLOBAL)\n"
            f'include("{ROOT}/symbian/toolchain/cmake/SymbianGraphics.cmake")\n'
            f'include("{ROOT}/symbian/toolchain/cmake/SymbianNativeSurface.cmake")\n'
            "add_executable(app app.cc)\n" + body + "\n" + extra
        )
        result = subprocess.run(
            [
                "cmake",
                "-G",
                "Ninja",
                "-S",
                str(tmp_path),
                "-B",
                str(tmp_path / "build"),
            ],
            capture_output=True,
            text=True,
        )
        result.stderr = " ".join(result.stderr.split())
        return result

    return run, sdk


def test_native_dependencies_configure_after_relocation(configure):
    run, _ = configure
    result = run("target_link_libraries(app PRIVATE Symbian::Audio)")
    assert result.returncode == 0, result.stderr


@pytest.mark.parametrize(
    "payload", ["proxies/common/common.dso", "include/native/common.h"]
)
def test_missing_transitive_payload_rejected_at_configuration(
    configure, payload
):
    run, sdk = configure
    (sdk / payload).unlink()
    result = run(
        "add_library(wrapper INTERFACE)\n"
        "target_link_libraries(wrapper INTERFACE Symbian::Audio)\n"
        "target_link_libraries(app PRIVATE wrapper)"
    )
    assert result.returncode != 0
    assert "Symbian::Common" in result.stderr
    assert "missing SDK payload" in result.stderr


@pytest.mark.parametrize(
    "extra, expected",
    [
        ("set(SYMBIAN_TARGET_ARCH armv7)", "ARM EABI toolchain"),
        ("set_property(TARGET app PROPERTY SYMBIAN_IMAGE_KERNEL eka1)", "EKA2"),
        (
            "target_compile_options(app PRIVATE -mfloat-abi=hard)",
            "incompatible flags",
        ),
        (
            "target_compile_options(app PRIVATE -fno-short-wchar)",
            "incompatible flags",
        ),
        ("target_compile_options(app PRIVATE -march=armv5t)", "conflicts"),
        (
            "set_source_files_properties(app.cc PROPERTIES "
            "COMPILE_OPTIONS -mfloat-abi=hard)",
            "incompatible flags",
        ),
        (
            "target_compile_options(app PRIVATE -mfpu=vfp)",
            "incompatible FPU flag",
        ),
    ],
)
def test_invalid_native_abi_and_kernel_rejected(configure, extra, expected):
    run, _ = configure
    result = run("target_link_libraries(app PRIVATE Symbian::Audio)", extra)
    assert result.returncode != 0
    assert expected in result.stderr


def test_blocked_facility_is_actionable_through_interface_dependency(configure):
    run, _ = configure
    result = run(
        "add_library(wrapper INTERFACE)\n"
        "target_link_libraries(wrapper INTERFACE Symbian::Blocked)\n"
        "target_link_libraries(app PRIVATE wrapper)"
    )
    assert result.returncode != 0
    assert "Missing frozen EABI evidence" in result.stderr


def test_selected_complete_firmware_rejects_missing_transitive_dll(
    configure, tmp_path
):
    run, _ = configure
    firmware = tmp_path / "firmware/sys/bin"
    firmware.mkdir(parents=True)
    (firmware / "audio.dll").write_text("presence control")
    result = run(
        "target_link_libraries(app PRIVATE Symbian::Audio)",
        f'set(SYMBIAN_NATIVE_FIRMWARE_DIR "{firmware.parent.parent}")',
    )
    assert result.returncode != 0
    assert "common.dll is absent" in result.stderr


@pytest.mark.parametrize("action", ["delete", "alter"])
def test_inventory_rejects_missing_or_modified_header(tmp_path, action):
    header = tmp_path / "include/native/public.h"
    header.parent.mkdir(parents=True)
    header.write_bytes(b"preserved header")
    digest = hashlib.sha256(header.read_bytes()).hexdigest()
    metadata = tmp_path / "share/symbian/native/inventory.json"
    metadata.parent.mkdir(parents=True)
    metadata.write_text(
        json.dumps(
            {
                "facilities": [],
                "headers": [
                    {"destination": "include/native/public.h", "sha256": digest}
                ],
            }
        )
    )
    if action == "delete":
        header.unlink()
    else:
        header.write_bytes(b"changed header")
    with pytest.raises(ValueError, match="(?i)native SDK payload"):
        validate_native_payload(tmp_path)


@pytest.mark.parametrize("damage", ["header", "definition", "proxy", "private"])
def test_qt_inventory_rejects_missing_payload(tmp_path, damage):
    header = tmp_path / "include/qt4/QtNetwork/QHostAddress"
    header.parent.mkdir(parents=True)
    header.write_bytes(b"public Qt header")
    definition = tmp_path / "share/symbian/qt/defs/QtNetworku.def"
    definition.parent.mkdir(parents=True)
    definition.write_bytes(b"EXPORTS\n")
    metadata = definition.parent.parent / "modules.json"
    metadata.write_text(
        json.dumps(
            {
                "modules": [
                    {
                        "target": "Symbian::QtNetwork",
                        "dll": "qtnetwork.dll",
                        "headers": {
                            "QtNetwork/QHostAddress": hashlib.sha256(
                                header.read_bytes()
                            ).hexdigest()
                        },
                        "source_definition": (
                            "src/s60installs/eabi/QtNetworku.def"
                        ),
                        "definition_sha256": hashlib.sha256(
                            definition.read_bytes()
                        ).hexdigest(),
                    }
                ]
            }
        )
    )
    if damage == "header":
        header.unlink()
    elif damage == "definition":
        definition.unlink()
    elif damage == "private":
        forbidden = tmp_path / "include/qt4/QtNetwork/private/qsocket_p.h"
        forbidden.parent.mkdir()
        forbidden.write_text("internal")
    with pytest.raises(ValueError, match="Qt .*|Qt import interface"):
        validate_qt_payload(tmp_path)
