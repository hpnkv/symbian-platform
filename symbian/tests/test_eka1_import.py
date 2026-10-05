"""Legacy EUSER import publication, real heap calls and failing controls."""

import json
import os
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.e32 import convert_eka1_executable, inspect_image
from symbian.emulator.configuration import resolve
from symbian.emulator.eka1 import run_probe
from symbian.sdk import build_import_proxy
from symbian.status import Code, StatusError
from symbian.toolchain.host_tools import llvm_tool

ROOT = Path(__file__).parents[2]
SYMBOLS = (
    "AllocLen__4UserPCv",
    "AllocSize__4UserRi",
    "Alloc__4Useri",
    "Copy__3MemPvPCvi",
    "Free__4UserPv",
)


@pytest.fixture(scope="module")
def imported_images(tmp_path_factory):
    """Builds a legacy-ordinal proxy and reproducible import/control images."""
    output = tmp_path_factory.mktemp("eka1-import-build")
    project = output / "project"
    shutil.copytree(ROOT / "examples/eka1_import_probe", project)
    compiler = llvm_tool("clang++")
    linker = llvm_tool("ld.lld", sibling=compiler.parent)
    proxy = build_import_proxy(
        project / "euser.def",
        list(SYMBOLS),
        "euser.dll",
        output / "proxy",
        str(compiler),
        str(linker),
    )
    manifest = project / "symbian.toml"
    manifest.write_text(
        manifest.read_text().replace(
            "../../.symbian/eka1-euser-proxy/euser.dso",
            proxy["artifact"],
        )
    )
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    data["configurePresets"][0]["toolchainFile"] = str(
        ROOT / "symbian/toolchain/cmake/symbian-arm.cmake"
    )
    presets.write_text(json.dumps(data))
    reports = {}
    for name, reason, corrupt in (
        ("normal", 7610, 0),
        ("changed", 7611, 0),
        ("corrupt", 7610, 1),
    ):
        reports[name] = toolchain.build(
            project,
            output / name,
            str(compiler),
            str(linker),
            cmake_variables={
                "SYMBIAN_EKA1_RESULT": str(reason),
                "SYMBIAN_EKA1_CORRUPT_COPY": str(corrupt),
            },
        )
    return reports, Path(proxy["artifact"]).read_bytes()


def test_legacy_import_metadata_and_elf_boundary(imported_images):
    reports, proxy = imported_images
    report = reports["normal"]
    assert report["reproducible"]
    assert report["schema"] == "symbian.e32-eka1-import/v1"
    info = inspect_image(Path(report["artifact"]))
    assert info == report["e32"] and info["kernel"] == "eka1"
    assert info["imports"][0]["dll"] == "euser.dll"
    slots = info["imports"][0]["slots"]
    assert {slot["ordinal"] for slot in slots} == {39, 41, 45, 243, 476}
    assert [slot["code_offset"] for slot in slots] == list(
        range(
            info["code_size"] - 24,
            info["code_size"] - 4,
            4,
        )
    )
    assert not info["data_size"] and not info["code_relocations"]
    elf = Path(report["linked_elf"]).read_bytes()
    assert (
        convert_eka1_executable(elf, 0xE0000761, [proxy])
        == Path(report["artifact"]).read_bytes()
    )
    with pytest.raises(StatusError):
        convert_eka1_executable(elf, 0xE0000761)
    assert len({value["sha256"] for value in reports.values()}) == 3


def test_import_section_truncation_is_rejected(imported_images, tmp_path):
    reports, _ = imported_images
    image = Path(reports["normal"]["artifact"]).read_bytes()
    damaged = tmp_path / "truncated.exe"
    damaged.write_bytes(image[:-1])
    with pytest.raises(StatusError) as caught:
        inspect_image(damaged)
    assert caught.value.code == Code.DATA_LOSS


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA1_GUEST"),
    reason="Named-firmware EKA1 acceptance is opt-in",
)
@pytest.mark.parametrize("backend", ("dynarmic", "dyncom"))
@pytest.mark.parametrize(
    "case,expected", (("normal", 7610), ("changed", 7611), ("corrupt", 41))
)
def test_original_euser_heap_and_copy(
    imported_images, tmp_path, backend, case, expected
):
    """Exercises original imports, heap cleanup and a corrupted-copy oracle."""
    reports, _ = imported_images
    image = Path(reports[case]["artifact"])
    resolution = resolve(
        root=ROOT,
        overrides={
            "firmware": "7610",
            "store": ROOT / ".symbian/firmware-store",
            "backend": backend,
        },
    )
    report = run_probe(
        image, tmp_path / "accepted", resolution, expected_reason=expected
    )
    assert report["accepted"] and report["golden_preserved"]
    assert report["process_exits"][0]["reason"] == expected
    if case != "normal":
        with pytest.raises(StatusError) as caught:
            run_probe(
                image, tmp_path / "rejected", resolution, expected_reason=7610
            )
        assert caught.value.code == Code.FAILED_PRECONDITION
