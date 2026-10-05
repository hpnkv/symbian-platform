"""Native EKA1 publication and ABI/runtime policy boundaries."""

from pathlib import Path

import pytest

from symbian import toolchain
from symbian.e32 import convert_eka1_executable, inspect_image
from symbian.status import Code, StatusError
from symbian.toolchain.host_tools import llvm_tool

ROOT = Path(__file__).parents[2]


@pytest.fixture(scope="module")
def eka1_images(tmp_path_factory):
    """Builds normal and changed-result images in independent CMake trees."""
    compiler = llvm_tool("clang++")
    linker = llvm_tool("ld.lld", sibling=compiler.parent)
    output = tmp_path_factory.mktemp("eka1-build")
    reports = {}
    for reason in (7610, 7611):
        reports[reason] = toolchain.build(
            ROOT / "examples/eka1_probe",
            output / str(reason),
            str(compiler),
            str(linker),
            cmake_variables={"SYMBIAN_EKA1_RESULT": str(reason)},
        )
    return reports


def test_eka1_reproducibility_and_metadata(eka1_images):
    normal, changed = eka1_images[7610], eka1_images[7611]
    assert normal["sha256"] != changed["sha256"]
    assert normal["reproducible"] and changed["reproducible"]
    assert normal["schema"] == "symbian.e32-eka1/v1"
    assert normal["target"]["e32_cpu"] == 0x2000
    assert normal["target"]["architecture"] == "armv5t"
    info = inspect_image(Path(normal["artifact"]))
    assert info == normal["e32"]
    assert info["kernel"] == "eka1"
    assert info["header_size"] == 124 and info["header_crc"] == 0
    assert info["secure_id"] == 0 and not info["imports"]
    assert not normal["runtime_verified"]
    with pytest.raises(StatusError) as caught:
        convert_eka1_executable(Path(normal["linked_elf"]).read_bytes(), 1)
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_eka1_cannot_be_packaged_as_sisx(eka1_images):
    from symbian.native import require_native

    with pytest.raises(StatusError) as caught:
        require_native().build_sis(
            Path(eka1_images[7610]["artifact"]).read_bytes(),
            0xE0000761,
            "EKA1 probe",
            "SDK",
            "eka1_probe.exe",
            (1, 0, 0),
        )
    assert caught.value.code == Code.UNIMPLEMENTED
    assert "legacy SIS" in str(caught.value)


def test_eka1_rejects_newer_isa_before_build(tmp_path):
    compiler = llvm_tool("clang++")
    with pytest.raises(StatusError) as caught:
        toolchain.build(
            ROOT / "examples/eka1_probe",
            tmp_path / "bad-isa",
            str(compiler),
            str(llvm_tool("ld.lld", sibling=compiler.parent)),
            architecture="armv6",
        )
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_verifier_refuses_other_fixture_before_creating_output(
    tmp_path,
    monkeypatch,
):
    from types import SimpleNamespace

    from symbian.emulator import eka1
    from symbian.emulator.configuration import Resolution, Settings

    other = SimpleNamespace(
        identity="unverified", device=SimpleNamespace(kernel="eka1")
    )
    monkeypatch.setattr(eka1, "selected", lambda _: (tmp_path, other))
    output = tmp_path / "untouched"
    with pytest.raises(StatusError) as caught:
        eka1.run_probe(
            tmp_path / "absent.exe",
            output,
            Resolution(settings=Settings(), origins={}, layers=[]),
        )
    assert caught.value.code == Code.FAILED_PRECONDITION
    assert not output.exists()


@pytest.mark.parametrize(
    "restriction", ("preserved-output", "eka2-image", "executive-map")
)
def test_verifier_preflight_protects_firmware_and_abi(
    eka1_images,
    tmp_path,
    monkeypatch,
    restriction,
):
    from types import SimpleNamespace

    from symbian.emulator import eka1
    from symbian.emulator.configuration import Resolution, Settings

    baseline = tmp_path / "preserved/instance"
    baseline.mkdir(parents=True)
    firmware = SimpleNamespace(
        identity=eka1.NOKIA_7610, device=SimpleNamespace(kernel="eka1")
    )
    monkeypatch.setattr(eka1, "selected", lambda _: (baseline, firmware))
    emulator = tmp_path / "emulator"
    emulator.write_bytes(b"must never run")
    image = Path(eka1_images[7610]["artifact"])
    output = tmp_path / "untouched"
    profile = "default"
    if restriction == "preserved-output":
        output = baseline / "untouched"
    elif restriction == "eka2-image":
        monkeypatch.setattr(eka1, "inspect_image", lambda _: {"kernel": "eka2"})
    else:
        profile = "rm807-113.010.1508"
    with pytest.raises(StatusError) as caught:
        eka1.run_probe(
            image,
            output,
            Resolution(
                settings=Settings(emulator=emulator, profile=profile),
                origins={},
                layers=[],
            ),
        )
    assert caught.value.code in (
        Code.INVALID_ARGUMENT,
        Code.FAILED_PRECONDITION,
    )
    assert not output.exists()
    assert not list(baseline.iterdir())
