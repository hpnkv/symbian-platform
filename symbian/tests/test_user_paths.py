"""User asset layout, retained private assets and CMake/Python selection."""

import json
from pathlib import Path

import pytest

from symbian.emulator.configuration import resolve
from symbian.packaging.signing import IdentityStore
from symbian.paths import asset_directory
from symbian.process import run
from symbian.project.sdk import discover_sdk
from symbian.status import StatusError


@pytest.fixture(autouse=True)
def isolated_home(tmp_path, monkeypatch):
    monkeypatch.setenv("HOME", str(tmp_path))
    for name in (
        "SYMBIAN_HOME",
        "SYMBIAN_SDK_MANIFEST",
        "XDG_CONFIG_HOME",
        "XDG_DATA_HOME",
        "XDG_CACHE_HOME",
    ):
        monkeypatch.delenv(name, raising=False)
    monkeypatch.chdir(tmp_path)


def test_defaults_share_one_root_without_creating_it(tmp_path):
    assert asset_directory("data") == tmp_path / ".symbian"
    assert asset_directory("config") == tmp_path / ".symbian/config"
    assert asset_directory("cache") == tmp_path / ".symbian/cache"
    assert not (tmp_path / ".symbian").exists()


def test_one_explicit_root_overrides_xdg(tmp_path, monkeypatch):
    monkeypatch.setenv("SYMBIAN_HOME", str(tmp_path / "owned"))
    monkeypatch.setenv("XDG_DATA_HOME", str(tmp_path / "xdg"))
    assert asset_directory("data") == tmp_path / "owned"
    assert asset_directory("config") == tmp_path / "owned/config"
    assert asset_directory("data") / "firmware" == tmp_path / "owned/firmware"


@pytest.mark.parametrize("name", ["SYMBIAN_HOME", "XDG_DATA_HOME"])
def test_relative_environment_roots_are_rejected(monkeypatch, name):
    monkeypatch.setenv(name, "relative")
    with pytest.raises(StatusError, match="must be absolute"):
        asset_directory("data")


def test_old_locations_are_not_used(tmp_path):
    old_data = tmp_path / ".local/share/symbian"
    (old_data / "firmware").mkdir(parents=True)
    (old_data / "signing").mkdir()
    old_config = tmp_path / ".config/symbian"
    old_config.mkdir(parents=True)
    (old_config / "emulator.json").write_text('{"firmware":"old"}')
    (old_config / "active-sdk.json").write_text('{"manifest":"old/sdk.json"}')
    assert resolve().settings.store == asset_directory("data") / "firmware"
    assert resolve().settings.firmware is None
    assert IdentityStore().directory == asset_directory("data") / "signing"
    with pytest.raises(StatusError, match="Install a local SDK"):
        discover_sdk()
    assert not (tmp_path / ".symbian").exists()


@pytest.mark.parametrize("layout", ["default", "xdg", "custom"])
def test_python_and_cmake_find_the_same_sdk(tmp_path, monkeypatch, layout):
    if layout == "xdg":
        monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "xdg"))
    elif layout == "custom":
        monkeypatch.setenv("SYMBIAN_HOME", str(tmp_path / "custom"))
    active = asset_directory("config") / "active-sdk.json"
    active.parent.mkdir(parents=True)
    manifest = tmp_path / "sdk/sdk.json"
    active.write_text(json.dumps({"manifest": str(manifest)}))
    assert discover_sdk() == manifest
    module = Path(__file__).parents[1] / "toolchain/cmake/SymbianSdk.cmake"
    script = tmp_path / "select.cmake"
    script.write_text(
        f'include("{module}")\nsymbian_select_sdk()\n'
        f'if(NOT SYMBIAN_SDK_PREFIX STREQUAL "{manifest.parent}")\n'
        '  message(FATAL_ERROR "Python and CMake selected different SDKs")\n'
        "endif()\n"
    )
    run(["cmake", "-P", str(script)], cwd=tmp_path)


def test_source_workspace_ignores_all_installed_sdk_selectors(
    tmp_path, monkeypatch
):
    root = tmp_path / "source"
    app = root / "examples/app"
    app.mkdir(parents=True)
    (app / "sdk-location.json").write_text('{"sdk":"/wrong-project-sdk"}')
    monkeypatch.setenv("SYMBIAN_SDK_MANIFEST", "/wrong-env-sdk/sdk.json")
    module = Path(__file__).parents[1] / "toolchain/cmake/SymbianSdk.cmake"
    script = tmp_path / "select-source.cmake"
    script.write_text(
        f'include("{module}")\n'
        f'set(SYMBIAN_SOURCE_WORKSPACE "{root}")\n'
        'set(SYMBIAN_SDK_PREFIX "/wrong-cache-sdk")\n'
        f'symbian_select_sdk("{app}")\n'
        f'set(expected "{root}/.symbian/workspace-inputs")\n'
        "if(NOT SYMBIAN_SDK_PREFIX STREQUAL expected)\n"
        '  message(FATAL_ERROR "Source selected installed SDK files")\n'
        "endif()\n"
    )
    run(["cmake", "-P", str(script)], cwd=tmp_path)
