"""Real SIS writer, CLI policy and independent installer integration."""

import json
import os
import shutil
from pathlib import Path

import pytest

from symbian import packaging, toolchain
from symbian._native import build_sis, inspect_sis
from symbian.cli.__main__ import main
from symbian.packaging.verification import verify_package
from symbian.status import Code, StatusError

PROJECT = Path(__file__).parents[2] / "examples/e32_probe"


@pytest.fixture(scope="module")
def image(tmp_path_factory):
    if not shutil.which("clang++") or not shutil.which("ld.lld"):
        pytest.skip("Clang and LLD required")
    root = tmp_path_factory.mktemp("sis-input")
    return Path(toolchain.build(PROJECT, root)["artifact"])


def test_cli_package_reproducibility_and_native_metadata(
    image, tmp_path, capsys
):
    for output in (tmp_path / "first", tmp_path / "second"):
        assert (
            main(
                [
                    "package",
                    "--project",
                    str(PROJECT),
                    "--artifact",
                    str(image),
                    "--output",
                    str(output),
                ]
            )
            == 0
        )
        report = json.loads(capsys.readouterr().out)["result"]
        assert report["reproducible"] and report["unsigned"]
        assert not report["runtime_verified"]
        assert report["sis"]["uid"] == 0xE0000809
        assert report["sis"]["executable_uid"] == 0xE0000808
        assert report["sis"]["target"] == "!:\\sys\\bin\\probe.exe"
    first = tmp_path / "first/probe.sis"
    assert first.read_bytes() == (tmp_path / "second/probe.sis").read_bytes()
    assert main(["inspect", str(first), "--format", "sis"]) == 0
    metadata = json.loads(capsys.readouterr().out)["result"]["metadata"]
    assert metadata == report["sis"]
    info = inspect_sis(first.read_bytes())
    with pytest.raises(AttributeError):
        info.options.uid = 1


@pytest.mark.parametrize(
    "change",
    [
        "uid = true",
        "version = [1, 2]",
        "version = [true, 0, 0]",
        "uid = 4294967296",
        'executable_name = "../victim.exe"',
    ],
)
def test_bad_manifest_is_a_status_and_writes_nothing(image, tmp_path, change):
    manifest = (PROJECT / "symbian.toml").read_text()
    key = change.split(" = ")[0]
    lines = manifest.splitlines()
    for index, line in enumerate(lines):
        if line.startswith(key + " = "):
            lines[index] = change
    (tmp_path / "symbian.toml").write_text("\n".join(lines))
    with pytest.raises(StatusError) as caught:
        packaging.package(tmp_path, image, tmp_path / "output")
    assert caught.value.code == Code.INVALID_ARGUMENT
    assert not (tmp_path / "output").exists()


def test_native_boundary_retains_checksum_failure_status(image):
    with pytest.raises(StatusError) as caught:
        build_sis(b"invalid E32", 0xE0000809, "Probe", "Research", "probe.exe")
    assert caught.value.code == Code.DATA_LOSS
    data = build_sis(
        image.read_bytes(), 0xE0000809, "Probe", "Research", "probe.exe"
    )
    with pytest.raises(StatusError) as caught:
        inspect_sis(data[:-1])
    assert caught.value.code == Code.DATA_LOSS


def test_unknown_package_field_is_not_ignored(image, tmp_path):
    (tmp_path / "symbian.toml").write_text(
        (PROJECT / "symbian.toml").read_text() + '\nsignature = "ignored"\n'
    )
    with pytest.raises(StatusError) as caught:
        packaging.package(tmp_path, image, tmp_path / "output")
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_missing_native_oracles_does_not_claim_installation(image, tmp_path):
    result = packaging.package(PROJECT, image, tmp_path / "package")
    with pytest.raises(StatusError) as caught:
        verify_package(
            Path(result["artifact"]),
            image,
            tmp_path / "missing",
            tmp_path / "checks",
        )
    assert caught.value.code == Code.NOT_FOUND


def test_inspection_bounds_host_file_read(tmp_path):
    path = tmp_path / "huge.sis"
    with path.open("wb") as stream:
        stream.truncate(16 * 1024 * 1024 + 4097)
    with pytest.raises(StatusError) as caught:
        packaging.inspect_package(path)
    assert caught.value.code == Code.RESOURCE_EXHAUSTED


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD to run native oracles",
)
def test_real_package_install_launch_uninstall_cli(image, tmp_path, capsys):
    result = packaging.package(PROJECT, image, tmp_path / "package")
    assert (
        main(
            [
                "toolchain",
                "verify-package",
                result["artifact"],
                "--executable",
                str(image),
                "--oracles-build",
                os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"],
                "--output",
                str(tmp_path / "checks"),
            ]
        )
        == 0
    )
    report = json.loads(capsys.readouterr().out)["result"]
    assert report["tests_passed"] == 35
    assert report["eka2l1_install_launch_verified"]
    assert report["historical_sis_checksums_verified"]
    assert report["registry_reload_verified"]
    assert report["uninstall_reinstall_verified"]
    assert not report["runtime_verified"]
    assert not report["phone_installation_verified"]
    for check in report["oracles"]:
        assert Path(check["results"]).is_file()
        assert Path(check["log"]).is_file()


@pytest.mark.parametrize(
    ("filename", "colliding_input"),
    [("report.json", "package"), ("expected-image.sha1", "executable")],
)
def test_verification_preserves_inputs_at_generated_output_paths(
    image, tmp_path, filename, colliding_input
):
    packaged = packaging.package(PROJECT, image, tmp_path / "package")
    package = Path(packaged["artifact"])
    executable = image
    output = tmp_path / "checks"
    output.mkdir()
    collision = output / filename
    original = (package if colliding_input == "package" else image).read_bytes()
    collision.write_bytes(original)
    if colliding_input == "package":
        package = collision
    else:
        executable = collision
    with pytest.raises(StatusError) as caught:
        verify_package(package, executable, tmp_path / "missing", output)
    assert caught.value.code == Code.INVALID_ARGUMENT
    assert collision.read_bytes() == original
