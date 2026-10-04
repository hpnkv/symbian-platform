"""Real SIS writer, CLI policy and independent installer integration."""

import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from symbian import packaging, toolchain
from symbian._native import build_sis, inspect_e32, inspect_sis, sign_sis
from symbian.packaging.verification import verify_package
from symbian.status import Code, StatusError
from symbian.tests.cli_json import main

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


def test_self_signed_package_verifies_signature_and_rejects_tampering(
    image, tmp_path
):
    openssl = shutil.which("openssl")
    if openssl is None:
        pytest.skip("OpenSSL command required for signing fixture")
    certificate = tmp_path / "signing.cer"
    key = tmp_path / "signing.key"
    subprocess.run(
        [
            openssl,
            "req",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-sha256",
            "-nodes",
            "-days",
            "1",
            "-subj",
            "/CN=Test Package",
            "-keyout",
            str(key),
            "-out",
            str(certificate),
        ],
        check=True,
        capture_output=True,
    )
    unsigned = build_sis(
        image.read_bytes(), 0xE0000809, "Probe", "Research", "probe.exe"
    )
    signed = sign_sis(unsigned, certificate.read_bytes(), key.read_bytes())
    assert not inspect_sis(unsigned).signed_package
    assert inspect_sis(signed).signed_package
    assert (
        inspect_sis(signed).executable_sha1
        == inspect_sis(unsigned).executable_sha1
    )
    with pytest.raises(StatusError):
        inspect_sis(signed[:-1])
    with pytest.raises(StatusError):
        sign_sis(unsigned, certificate.read_bytes(), b"wrong key")
    report = packaging.package(
        PROJECT,
        image,
        tmp_path / "signed-package",
        signing_certificate=certificate,
        signing_key=key,
    )
    assert report["signed"] and not report["unsigned"]
    assert packaging.inspect_package(Path(report["artifact"]))["signed_package"]


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
        stream.truncate(16 * 1024 * 1024 + 65537)
    with pytest.raises(StatusError) as caught:
        packaging.inspect_package(path)
    assert caught.value.code == Code.RESOURCE_EXHAUSTED


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_APP_SDK")
    or not os.environ.get("SYMBIAN_REGISTRATION_TEST_IMAGE"),
    reason="Set SYMBIAN_APP_SDK and SYMBIAN_REGISTRATION_TEST_IMAGE",
)
def test_registered_package_compiles_real_menu_resources(tmp_path, monkeypatch):
    monkeypatch.setenv("SYMBIAN_SDK_MANIFEST", os.environ["SYMBIAN_APP_SDK"])
    image = Path(os.environ["SYMBIAN_REGISTRATION_TEST_IMAGE"])
    uid3 = inspect_e32(image.read_bytes()).uid3
    (tmp_path / "symbian.toml").write_text(
        f'[package]\nuid = {uid3}\nname = "Resource Probe"\n'
        'vendor = "Symbian research"\nexecutable_name = "probe.exe"\n'
        "version = [1, 0, 0]\n\n[application]\n"
        'caption = "Resource Probe"\nshort_caption = "Probe"\n'
    )
    result = packaging.package(tmp_path, image, tmp_path / "output")
    sis = result["sis"]
    assert sis["application_registered"] is True
    assert [file["target"] for file in sis["files"]] == [
        "!:\\sys\\bin\\probe.exe",
        "!:\\private\\10003a3f\\import\\apps\\probe_reg.rsc",
        "!:\\resource\\apps\\probe_loc.rsc",
    ]
    assert all(file["size"] > 24 for file in sis["files"][1:])
    (tmp_path / "symbian.toml").write_text(
        (tmp_path / "symbian.toml")
        .read_text()
        .replace('caption = "Resource Probe"', "caption = 'Bad\"Name'")
    )
    with pytest.raises(StatusError) as caught:
        packaging.package(tmp_path, image, tmp_path / "rejected")
    assert caught.value.code == Code.INVALID_ARGUMENT
    assert not (tmp_path / "rejected").exists()


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_APP_SDK")
    or not os.environ.get("SYMBIAN_REGISTRATION_TEST_IMAGE"),
    reason="Set SYMBIAN_APP_SDK and SYMBIAN_REGISTRATION_TEST_IMAGE",
)
def test_localized_svg_package_is_reproducible(tmp_path, monkeypatch):
    monkeypatch.setenv("SYMBIAN_SDK_MANIFEST", os.environ["SYMBIAN_APP_SDK"])
    image = Path(os.environ["SYMBIAN_REGISTRATION_TEST_IMAGE"])
    uid3 = inspect_e32(image.read_bytes()).uid3
    (tmp_path / "icon.svg").write_text(
        '<svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 16 16">'
        '<circle cx="8" cy="8" r="7" fill="#456"/></svg>'
    )
    (tmp_path / "symbian.toml").write_text(
        f'[package]\nuid = {uid3}\nname = "Locale Probe"\n'
        'vendor = "Symbian research"\nexecutable_name = "probe.exe"\n'
        "version = [1, 0, 0]\n\n[application]\n"
        'caption = "Locale Probe"\nshort_caption = "Probe"\n'
        'icon = "icon.svg"\n\n[application.localizations.fr]\n'
        'caption = "Compteur"\nshort_caption = "Compteur"\n'
        "\n[application.localizations.de]\n"
        'caption = "Zähler"\nshort_caption = "Zähler"\n',
        encoding="utf-8",
    )
    report = packaging.package(tmp_path, image, tmp_path / "output")
    targets = [file["target"] for file in report["sis"]["files"]]
    assert targets == [
        "!:\\sys\\bin\\probe.exe",
        "!:\\private\\10003a3f\\import\\apps\\probe_reg.rsc",
        "!:\\resource\\apps\\probe_loc.rsc",
        "!:\\resource\\apps\\probe_loc.r02",
        "!:\\resource\\apps\\probe_loc.r03",
        "!:\\resource\\apps\\probe.mif",
    ]
    assert report["application_asset_sha256"]["icon.svg"]
    assert (
        packaging.package(tmp_path, image, tmp_path / "output")["sha256"]
        == report["sha256"]
    )
    manifest = tmp_path / "symbian.toml"
    original = manifest.read_text()
    manifest.write_text(
        original.replace("localizations.fr", "localizations.zz")
    )
    with pytest.raises(StatusError) as caught:
        packaging.package(tmp_path, image, tmp_path / "bad-locale")
    assert caught.value.code == Code.INVALID_ARGUMENT
    manifest.write_text(
        original.replace('icon = "icon.svg"', 'icon = "../icon.svg"')
    )
    with pytest.raises(StatusError) as caught:
        packaging.package(tmp_path, image, tmp_path / "bad-path")
    assert caught.value.code == Code.INVALID_ARGUMENT
    manifest.write_text(original)
    (tmp_path / "icon.svg").write_text("<not-svg/>")
    with pytest.raises(StatusError) as caught:
        packaging.package(tmp_path, image, tmp_path / "bad-icon")
    assert caught.value.code == Code.INVALID_ARGUMENT


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
