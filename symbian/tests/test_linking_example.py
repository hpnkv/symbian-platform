"""Real static/DLL application linking, relocation and package controls."""

import json
import os
import shutil
import subprocess
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.e32 import inspect_image
from symbian.packaging.packages import inspect_package, package
from symbian.project.libraries import package_libraries
from symbian.project.sdk import AppSdk
from symbian.status import StatusError

ROOT = Path(__file__).resolve().parents[2]


@pytest.fixture(params=["armv5t", "armv6"])
def linking(tmp_path, request):
    """Builds the actual example twice from a disposable project."""
    manifest = os.environ.get("SYMBIAN_APP_SDK")
    if not manifest:
        pytest.skip("Set SYMBIAN_APP_SDK to a complete installed SDK")
    sdk = AppSdk.load(Path(manifest))
    project = tmp_path / "project with spaces"
    shutil.copytree(ROOT / "examples/linking_app", project)
    (project / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = toolchain.build(
        project,
        tmp_path / "built with spaces",
        str(sdk.compiler),
        str(sdk.linker),
        architecture=request.param,
    )
    return project, report, tmp_path


def test_static_and_dynamic_example_packages_after_relocation(linking):
    project, report, temporary = linking
    assert report["reproducible"]
    imports = {item["dll"] for item in report["e32"]["imports"]}
    assert "linking_dynamic.dll" in imports
    database = json.loads(Path(report["compile_commands"]).read_text())
    assert {"app.cc", "static.cc", "dynamic.cc"}.issubset(
        {Path(item["file"]).name for item in database}
    )
    original = Path(report["artifact"])
    relocated = temporary / "relocated application"
    relocated.mkdir()
    shutil.copyfile(original, relocated / original.name)
    shutil.copyfile(
        original.with_suffix(".libraries.json"),
        relocated / original.with_suffix(".libraries.json").name,
    )
    shutil.copytree(original.parent / "libraries", relocated / "libraries")
    artifact = relocated / original.name
    libraries = package_libraries(artifact)
    assert [target for target, _ in libraries] == [
        "!:\\sys\\bin\\linking_dynamic.dll"
    ]
    dll = inspect_image(relocated / "libraries/linking_dynamic.dll")
    assert dll["dll"]
    assert dll["architecture"] == report["e32"]["architecture"]
    built = package(project, artifact, temporary / "package")
    info = inspect_package(Path(built["artifact"]))
    assert [file["target"] for file in info["files"]] == [
        "!:\\sys\\bin\\linking_app.exe",
        "!:\\sys\\bin\\linking_dynamic.dll",
    ]
    assert not info["application_registered"]
    assert info["files"][1]["size"] == len(libraries[0][1])


def test_altered_bundled_dll_is_rejected(linking):
    project, report, temporary = linking
    artifact = Path(report["artifact"])
    (artifact.parent / "libraries/linking_dynamic.dll").write_bytes(
        b"altered DLL"
    )
    with pytest.raises(StatusError, match="Bundled DLL digest mismatch"):
        package(project, artifact, temporary / "package")


@pytest.mark.parametrize(
    ("selection", "expected"),
    [
        (None, "linked project DLLs require an explicit packaging choice"),
        ("INVALID", "PROJECT_DLLS must be BUNDLE or RUNTIME"),
    ],
)
def test_project_dll_packaging_choice_is_required(
    tmp_path, selection, expected
):
    manifest = os.environ.get("SYMBIAN_APP_SDK")
    if not manifest:
        pytest.skip("Set SYMBIAN_APP_SDK to a complete installed SDK")
    sdk = AppSdk.load(Path(manifest))
    project = tmp_path / "project"
    shutil.copytree(ROOT / "examples/linking_app", project)
    cmake_file = project / "CMakeLists.txt"
    contents = cmake_file.read_text().replace(
        "PROJECT_DLLS BUNDLE",
        "" if selection is None else f"PROJECT_DLLS {selection}",
    )
    cmake_file.write_text(contents)
    result = subprocess.run(
        [
            str(sdk.prefix / "bin/cmake"),
            "-G",
            "Ninja",
            "-S",
            str(project),
            "-B",
            str(tmp_path / "build"),
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
        ],
        text=True,
        capture_output=True,
        check=False,
    )
    assert result.returncode != 0
    assert expected in result.stdout + result.stderr


def test_runtime_supplied_project_dll_is_not_packaged(tmp_path):
    manifest = os.environ.get("SYMBIAN_APP_SDK")
    if not manifest:
        pytest.skip("Set SYMBIAN_APP_SDK to a complete installed SDK")
    sdk = AppSdk.load(Path(manifest))
    project = tmp_path / "project"
    shutil.copytree(ROOT / "examples/linking_app", project)
    cmake_file = project / "CMakeLists.txt"
    cmake_file.write_text(
        cmake_file.read_text().replace(
            "PROJECT_DLLS BUNDLE", "PROJECT_DLLS RUNTIME"
        )
    )
    (project / "sdk-location.json").write_text(
        json.dumps({"sdk": str(sdk.prefix)})
    )
    report = toolchain.build(
        project,
        tmp_path / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
    )
    assert "linking_dynamic.dll" in {
        item["dll"] for item in report["e32"]["imports"]
    }
    assert package_libraries(Path(report["artifact"])) == []
    built = package(project, Path(report["artifact"]), tmp_path / "package")
    assert [
        item["target"]
        for item in inspect_package(Path(built["artifact"]))["files"]
    ] == ["!:\\sys\\bin\\linking_app.exe"]
