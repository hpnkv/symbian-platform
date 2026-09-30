"""GUI source staging policy and optional real SDK/link validation."""

import hashlib
import json
import os
import shlex
import shutil
from pathlib import Path

import pytest

from symbian import packaging, toolchain
from symbian.cli.__main__ import main
from symbian.packaging.verification import verify_gui_package
from symbian.sdk import staging
from symbian.status import Code, StatusError
from symbian.toolchain.verification import verify_gui

PROJECT = Path(__file__).parents[2] / "examples/gui_app"
SOURCES = os.environ.get("SYMBIAN_GUI_SOURCE_ROOT")
ORACLES = os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD")


@pytest.fixture
def source_profile(tmp_path):
    project = tmp_path / "project"
    sources = tmp_path / "sources"
    project.mkdir()
    sources.mkdir()
    manifest = {
        "schema": "symbian.gui-source-sdk/v1",
        "repositories": {"fixture": "0" * 40},
        "headers": {},
        "imports": {},
    }
    header = sources / "MixedCase.H"
    header.write_text("// Preserved source header\n")
    manifest["headers"]["fixture.h"] = {
        "source": header.name,
        "sha256": hashlib.sha256(header.read_bytes()).hexdigest(),
    }
    for dll in ("euser.dll", "ws32.dll"):
        definition = sources / (dll + ".def")
        definition.write_text("EXPORTS\nFunction @ 7 NONAME\n")
        manifest["imports"][dll] = {
            "definition": definition.name,
            "definition_sha256": hashlib.sha256(
                definition.read_bytes()
            ).hexdigest(),
            "symbols": ["Function"],
        }
    (project / "sdk.json").write_text(json.dumps(manifest))
    return project, sources, tmp_path / "stage", manifest


def test_staging_keeps_original_headers_and_records_unverified_scope(
    source_profile, monkeypatch
):
    project, sources, output, _ = source_profile
    calls = []

    def proxy(definition, symbols, dll, destination):
        calls.append((definition, symbols, dll, destination))
        return {"fixture": dll}

    monkeypatch.setattr(staging, "build_import_proxy", proxy)
    first = staging.prepare_gui_sdk(project, sources, output)
    second = staging.prepare_gui_sdk(project, sources, output)
    assert first == second
    assert len(calls) == 4
    assert (output / "include/fixture.h").is_symlink()
    assert (output / "include/fixture.h").resolve() == sources / "MixedCase.H"
    assert not first["sdk_verified"]
    assert not first["repository_revisions_verified"]
    assert not first["runtime_verified"]
    assert first == json.loads((output / "sdk-report.json").read_text())


@pytest.mark.parametrize(
    "mutation",
    ["nested_type", "selection_type", "digest", "escape", "dot", "revision"],
)
def test_bad_profiles_fail_before_publishing(source_profile, mutation):
    project, sources, output, manifest = source_profile
    if mutation == "nested_type":
        manifest["headers"]["fixture.h"] = []
    elif mutation == "selection_type":
        manifest["imports"]["ws32.dll"]["symbols"] = "Function"
    elif mutation == "digest":
        (sources / "MixedCase.H").write_text("// Altered input\n")
    elif mutation == "escape":
        manifest["headers"]["../escape.h"] = manifest["headers"].pop(
            "fixture.h"
        )
    elif mutation == "dot":
        manifest["headers"]["."] = manifest["headers"].pop("fixture.h")
    else:
        manifest["repositories"]["fixture"] = 17
    (project / "sdk.json").write_text(json.dumps(manifest))
    with pytest.raises(StatusError) as caught:
        staging.prepare_gui_sdk(project, sources, output)
    assert caught.value.code == (
        Code.DATA_LOSS if mutation == "digest" else Code.INVALID_ARGUMENT
    )
    assert not output.exists()


def test_invalid_second_dll_selection_does_not_stage_first(source_profile):
    project, sources, output, manifest = source_profile
    manifest["imports"]["ws32.dll"]["symbols"] = ["Missing"]
    (project / "sdk.json").write_text(json.dumps(manifest))
    with pytest.raises(StatusError):
        staging.prepare_gui_sdk(project, sources, output)
    assert not output.exists()


@pytest.mark.parametrize("redirect", ["occupied", "include", "proxy"])
def test_existing_output_cannot_redirect_or_replace_inputs(
    source_profile, redirect
):
    project, sources, output, _ = source_profile
    output.mkdir()
    original = (sources / "MixedCase.H").read_bytes()
    if redirect == "occupied":
        (output / "include").mkdir()
        (output / "include/fixture.h").write_bytes(b"existing")
    elif redirect == "include":
        (output / "include").symlink_to(sources, target_is_directory=True)
    else:
        (output / "euser").symlink_to(sources, target_is_directory=True)
    with pytest.raises(StatusError):
        staging.prepare_gui_sdk(project, sources, output)
    assert (sources / "MixedCase.H").read_bytes() == original
    if redirect == "occupied":
        assert (output / "include/fixture.h").read_bytes() == b"existing"


def test_malformed_profile_cli_returns_canonical_status(source_profile, capsys):
    project, sources, output, _ = source_profile
    (project / "sdk.json").write_text('{"schema": []}')
    assert (
        main(
            [
                "toolchain",
                "prepare-gui-sdk",
                "--project",
                str(project),
                "--sources-root",
                str(sources),
                "--output",
                str(output),
            ]
        )
        == 1
    )
    assert json.loads(capsys.readouterr().out)["status"]["name"] == (
        "INVALID_ARGUMENT"
    )


@pytest.fixture(scope="module")
def gui(tmp_path_factory):
    if not SOURCES or not shutil.which("clang++") or not shutil.which("ld.lld"):
        pytest.skip("Supply SYMBIAN_GUI_SOURCE_ROOT and Clang/LLD")
    root = tmp_path_factory.mktemp("GUI project and SDK with spaces")
    project = root / "project"
    shutil.copytree(PROJECT, project)
    prepared = staging.prepare_gui_sdk(project, Path(SOURCES), root / "sdk")
    manifest = project / "symbian.toml"
    text = manifest.read_text().replace(
        'cmake_preset = "symbian-pic"', 'cmake_preset = "gui-sdk"'
    )
    for dll, proxy in prepared["proxies"].items():
        text = text.replace(
            f'"../../.symbian/gui-sdk/{dll[:-4]}/{dll[:-4]}.dso"',
            json.dumps(proxy["artifact"]),
        )
    manifest.write_text(text)
    (project / "CMakeUserPresets.json").write_text(
        json.dumps(
            {
                "version": 6,
                "configurePresets": [
                    {
                        "name": "gui-sdk",
                        "inherits": "symbian-pic",
                        "cacheVariables": {
                            "SYMBIAN_GUI_SDK_INCLUDE": prepared["include"]
                        },
                    }
                ],
            }
        )
    )
    return toolchain.build(project, root / "build"), prepared


def test_gui_real_headers_link_without_division_helpers_or_host_runtime(gui):
    report, prepared = gui
    assert report["reproducible"]
    assert prepared["header_count"] == 92
    assert report["e32"]["uid3"] == 0xE0000811
    assert not report["e32"]["dll"]
    assert {
        item["dll"]: len(item["slots"]) for item in report["e32"]["imports"]
    } == {"euser.dll": 10, "ws32.dll": 28}
    assert not report["runtime_verified"]
    assert not report["import_execution_verified"]
    assert any(path.endswith("W32STD.H") for path in report["inputs"])
    database = json.loads(Path(report["compile_commands"]).read_text())
    for row in database:
        if row["file"].endswith(".cc"):
            arguments = shlex.split(row["command"])
            for flag in (
                "-std=c++20",
                "-fno-exceptions",
                "-nostdinc",
                "-g",
                "-gdwarf-4",
                "-O1",
                "-D_UNICODE",
            ):
                assert flag in arguments


@pytest.mark.skipif(not ORACLES, reason="Supply historical oracle build")
def test_gui_original_image_validation_keeps_all_execution_flags_false(
    gui, tmp_path
):
    report, _ = gui
    result = verify_gui(
        Path(report["artifact"]), Path(ORACLES), tmp_path / "checks"
    )
    assert result["tests_passed"] == 8
    assert result["historical_image_validation_passed"]
    for key in (
        "gui_execution_verified",
        "debugger_attachment_verified",
        "import_execution_verified",
        "symbian_loader_verified",
        "runtime_verified",
    ):
        assert not result[key]
    assert result["sha256"] == report["sha256"]


def test_gui_validator_rejects_integer_probe(tmp_path):
    if not shutil.which("clang++") or not shutil.which("ld.lld"):
        pytest.skip("Clang/LLD required")
    report = toolchain.build(PROJECT.parent / "e32_probe", tmp_path / "build")
    with pytest.raises(StatusError) as caught:
        verify_gui(Path(report["artifact"]), tmp_path, tmp_path / "checks")
    assert caught.value.code == Code.INVALID_ARGUMENT
    assert not (tmp_path / "checks").exists()


def test_imported_gui_package_keeps_native_digest_and_rejects_dll(
    gui, tmp_path
):
    report, _ = gui
    executable = Path(report["artifact"])
    packaged = packaging.package(PROJECT, executable, tmp_path / "package")
    assert packaged["sis"]["uid"] == 0xE0000812
    assert packaged["sis"]["executable_uid"] == 0xE0000811
    assert (
        packaged["sis"]["executable_sha1"]
        == hashlib.sha1(executable.read_bytes()).hexdigest()
    )
    assert not packaged["runtime_verified"]
    assert packaged["sis"]["target"] == "!:\\sys\\bin\\gui_app.exe"
    dll = toolchain.build(PROJECT.parent / "dll_probe", tmp_path / "dll")
    with pytest.raises(StatusError) as caught:
        packaging.package(PROJECT, Path(dll["artifact"]), tmp_path / "bad")
    assert caught.value.code == Code.UNIMPLEMENTED
    assert not (tmp_path / "bad").exists()


def test_mismatched_equal_size_gui_payload_is_rejected_before_installation(
    gui, tmp_path
):
    report, _ = gui
    original = Path(report["artifact"])
    packaged = packaging.package(PROJECT, original, tmp_path / "package")
    source = Path(
        next(path for path in report["inputs"] if path.endswith("/app.cc"))
    ).parent
    project = tmp_path / "modified-project"
    shutil.copytree(source, project)
    app = project / "app.cc"
    app.write_text(app.read_text().replace("0x00e8ebf2", "0x00e8ebf3"))
    changed = toolchain.build(project, tmp_path / "changed")
    assert changed["sha256"] != report["sha256"]
    assert Path(changed["artifact"]).stat().st_size == original.stat().st_size
    with pytest.raises(StatusError) as caught:
        verify_gui_package(
            Path(packaged["artifact"]),
            Path(changed["artifact"]),
            tmp_path / "missing",
            tmp_path / "checks",
        )
    assert caught.value.code == Code.INVALID_ARGUMENT
    assert not (tmp_path / "checks").exists()


@pytest.mark.skipif(
    not ORACLES, reason="Supply historical/GUI installer oracles"
)
def test_gui_install_reload_remove_reinstall_and_missing_service_controls(
    gui, tmp_path, capsys
):
    report, _ = gui
    packaged = packaging.package(
        PROJECT, Path(report["artifact"]), tmp_path / "package"
    )
    assert (
        main(
            [
                "toolchain",
                "verify-gui-package",
                packaged["artifact"],
                "--executable",
                report["artifact"],
                "--oracles-build",
                ORACLES,
                "--output",
                str(tmp_path / "checks"),
            ]
        )
        == 0
    )
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["tests_passed"] == 17
    for flag in (
        "eka2l1_install_verified",
        "registry_reload_verified",
        "uninstall_reinstall_verified",
    ):
        assert result[flag]
    assert not result["missing_system_libraries_rejected"]
    assert result["unresolved_import_slots_observed"] == 38
    assert result["process_creation_without_system_libraries_observed"]
    assert result["cpu_instructions_executed"] == 0
    for flag in (
        "gui_execution_verified",
        "import_execution_verified",
        "debugger_attachment_verified",
        "symbian_loader_verified",
        "runtime_verified",
        "phone_installation_verified",
    ):
        assert not result[flag]
