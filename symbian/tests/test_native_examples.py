"""Real original-header example builds and eager ordinal pointer controls."""

import json
import os
import shutil
import struct
from pathlib import Path

import pytest

from symbian.e32 import convert_imported_executable, inspect_image
from symbian.packaging.packages import inspect_package, package
from symbian.project.sdk import AppSdk
from symbian.status import StatusError
from symbian.toolchain import build

ROOT = Path(__file__).resolve().parents[2]
SDK = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(not SDK, reason="Set SYMBIAN_APP_SDK")


@pytest.fixture(scope="module", params=["armv5t", "armv6"])
def native_examples(request, tmp_path_factory):
    """Builds original native consumers from a relocated project directory."""
    sdk = AppSdk.load(Path(SDK))
    temporary = tmp_path_factory.mktemp("native public examples")
    reports = {}
    for name in (
        "audio_app_classic",
        "bitmap_app_classic",
        "image_app_classic",
        "vibra_app_classic",
    ):
        source = temporary / name
        shutil.copytree(ROOT / "examples" / name, source)
        (source / "sdk-location.json").write_text(
            json.dumps({"sdk": str(sdk.prefix)})
        )
        reports[name] = build(
            source,
            temporary / (name + " output"),
            str(sdk.compiler),
            str(sdk.linker),
            architecture=request.param,
        )
    return request.param, reports


def test_games_examples_compile_link_and_convert(native_examples):
    architecture, reports = native_examples
    expected = {
        "audio_app_classic": "mediaclientaudiostream.dll",
        "bitmap_app_classic": "fbscli.dll",
        "image_app_classic": "imageconversion.dll",
        "vibra_app_classic": "hwrmvibraclient.dll",
    }
    for name, report in reports.items():
        assert report["reproducible"]
        image = inspect_image(Path(report["artifact"]))
        assert image["architecture"] == architecture
        assert expected[name] in {b["dll"] for b in image["imports"]}
        assert not report["runtime_verified"]


def test_readonly_vtable_imports_keep_ordinals_and_reject_changes(
    native_examples,
):
    _, reports = native_examples
    report = reports["image_app_classic"]
    elf = Path(report["linked_elf"]).read_bytes()
    proxies = [
        Path(p).read_bytes() for p in report["inputs"] if p.endswith(".dso")
    ]
    image = Path(report["artifact"]).read_bytes()
    uid = report["e32"]["uid3"]
    assert convert_imported_executable(elf, proxies, uid) == image
    start = struct.unpack_from("<I", elf, 32)[0]
    stride, count = struct.unpack_from("<HH", elf, 46)
    sections = [
        struct.unpack_from("<10I", elf, start + i * stride)
        for i in range(count)
    ]
    dynamic = next(s for s in sections if s[1] == 9 and s[2] == 2)
    symbols = sections[dynamic[6]]
    # Find a function imported directly into a vtable, with no PLT slot.
    for offset in range(dynamic[4], dynamic[4] + dynamic[5], 8):
        location, info = struct.unpack_from("<II", elf, offset)
        symbol = symbols[4] + (info >> 8) * 16
        if info & 255 == 2 and elf[symbol + 12] == 0x12:
            break
    else:
        pytest.fail("No original CActive read-only function import")
    owner = next(
        s for s in sections if s[1] == 1 and s[3] <= location < s[3] + s[5]
    )
    pointer = owner[4] + location - owner[3]
    assert struct.unpack_from("<I", elf, pointer)[0] == 0
    mutations = [(offset, location + 1), (pointer, 4), (offset + 4, info + 1)]
    for position, value in mutations:
        changed = bytearray(elf)
        struct.pack_into("<I", changed, position, value)
        with pytest.raises(StatusError):
            convert_imported_executable(bytes(changed), proxies, uid)


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_qt_standard_module_example_links_original_imports(
    tmp_path, architecture
):
    """Checks Qt module headers and transitive imports in one real app."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "qt modules"
    shutil.copytree(ROOT / "examples/qt_modules_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {
        "qtcore.dll",
        "qtnetwork.dll",
        "qtsql.dll",
        "qtxml.dll",
        "qtwebkit.dll",
        "qtopengl.dll",
    } <= imports
    assert report["e32"]["architecture"] == architecture


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_qt_mobility_example_links_frozen_modules(tmp_path, architecture):
    """Builds a public Contacts/Location consumer through the relocated SDK."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "qt mobility"
    shutil.copytree(ROOT / "examples/qt_mobility_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {"qtcontacts.dll", "qtlocation.dll", "qtcore.dll"} <= imports
    assert report["e32"]["architecture"] == architecture
    assert not report["runtime_verified"]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_open_c_example_links_original_libc(tmp_path, architecture):
    """Builds the original Open C headers and complete libc import interface."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "openc classic"
    shutil.copytree(ROOT / "examples/openc_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {"libc.dll", "libpthread.dll", "euser.dll"} <= imports
    assert report["e32"]["architecture"] == architecture
    assert not report["runtime_verified"]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_apparc_example_links_original_application_service(
    tmp_path, architecture
):
    """Builds a real AppArc session consumer against frozen native imports."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "apparc classic"
    shutil.copytree(ROOT / "examples/apparc_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {"apgrfx.dll", "euser.dll"} <= imports
    assert report["e32"]["architecture"] == architecture
    assert not report["runtime_verified"]
    if architecture == "armv6":
        built = package(source, Path(report["artifact"]), tmp_path / "package")
        files = inspect_package(Path(built["artifact"]))["files"]
        assert [item["target"] for item in files] == [
            "!:\\sys\\bin\\apparc_app_classic.exe"
        ]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_central_repository_example_links_original_client(
    tmp_path, architecture
):
    """Compiles and converts a read-only Central Repository consumer."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "central repository classic"
    shutil.copytree(ROOT / "examples/central_repository_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {"centralrepository.dll", "euser.dll"} <= imports
    assert report["e32"]["architecture"] == architecture
    assert not report["runtime_verified"]
    if architecture == "armv6":
        built = package(source, Path(report["artifact"]), tmp_path / "package")
        files = inspect_package(Path(built["artifact"]))["files"]
        assert [item["target"] for item in files] == [
            "!:\\sys\\bin\\central_repository_app_classic.exe"
        ]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_bafl_example_propagates_file_and_stream_dependencies(
    tmp_path, architecture
):
    """Checks the read-only BAFL consumer's transitive frozen imports."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "bafl classic"
    shutil.copytree(ROOT / "examples/bafl_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {"bafl.dll", "efsrv.dll", "estor.dll", "euser.dll"} <= imports
    assert report["e32"]["architecture"] == architecture
    assert not report["runtime_verified"]
    if architecture == "armv6":
        built = package(source, Path(report["artifact"]), tmp_path / "package")
        files = inspect_package(Path(built["artifact"]))["files"]
        assert [item["target"] for item in files] == [
            "!:\\sys\\bin\\bafl_app_classic.exe"
        ]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_calendar_example_links_original_interim_api(tmp_path, architecture):
    """Checks a read-only calendar-session consumer and its frozen imports."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "calendar classic"
    shutil.copytree(ROOT / "examples/calendar_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {"calinterimapi.dll", "euser.dll"} <= imports
    assert report["e32"]["architecture"] == architecture
    assert not report["runtime_verified"]
    if architecture == "armv6":
        built = package(source, Path(report["artifact"]), tmp_path / "package")
        files = inspect_package(Path(built["artifact"]))["files"]
        assert [item["target"] for item in files] == [
            "!:\\sys\\bin\\calendar_app_classic.exe"
        ]


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
def test_messaging_example_links_original_session_api(tmp_path, architecture):
    """Checks a message-server session consumer and frozen msgs imports."""
    sdk = AppSdk.load(Path(SDK))
    source = tmp_path / "messaging classic"
    shutil.copytree(ROOT / "examples/messaging_app_classic", source)
    (source / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = build(
        source,
        tmp_path / "output",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=architecture,
    )
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert {"msgs.dll", "euser.dll"} <= imports
    assert report["e32"]["architecture"] == architecture
    assert not report["runtime_verified"]
    if architecture == "armv6":
        built = package(source, Path(report["artifact"]), tmp_path / "package")
        files = inspect_package(Path(built["artifact"]))["files"]
        assert [item["target"] for item in files] == [
            "!:\\sys\\bin\\messaging_app_classic.exe"
        ]
