"""ARM target selection, ABI metadata, rebuild and launch admission."""

import json
import os
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.status import Code, StatusError
from symbian.toolchain.architecture import (
    project_architecture,
    require_execution_architecture,
    target,
)

ROOT = Path(__file__).parents[2]
TOOLS = all(
    shutil.which(name) for name in ("clang++", "ld.lld", "cmake", "ninja")
)


def test_default_legacy_and_single_project_authority(tmp_path):
    (tmp_path / "CMakePresets.json").write_text(
        json.dumps({"configurePresets": [{"name": "app"}]})
    )
    assert project_architecture(tmp_path, {}, "app") == "armv6"
    assert target("armv6").e32_cpu == 0x2002
    (tmp_path / "symbian-project.json").write_text(
        json.dumps({"preferences": {"architecture": "armv5t"}})
    )
    assert project_architecture(tmp_path, {}, "app") == "armv5t"
    assert (
        project_architecture(tmp_path, {"architecture": "armv6"}, "app")
        == "armv6"
    )
    with pytest.raises(StatusError) as failure:
        target("armv7")
    assert failure.value.code == Code.INVALID_ARGUMENT


@pytest.mark.parametrize("architecture", ["armv7", "x86", "aarch64"])
def test_unsupported_execution_has_recovery(architecture):
    with pytest.raises(StatusError) as failure:
        require_execution_architecture(architecture, "dynarmic")
    assert failure.value.code == Code.FAILED_PRECONDITION
    assert "rebuild" in str(failure.value).lower()
    assert "No guest was started" in str(failure.value)


@pytest.mark.skipif(not TOOLS, reason="Clang/LLD/CMake/Ninja required")
def test_same_tree_architecture_change_rebuilds_and_matches_metadata(tmp_path):
    project = tmp_path / "project"
    shutil.copytree(ROOT / "probes/e32_probe", project)
    output = tmp_path / "build"
    compiler = str(Path("/opt/homebrew/opt/llvm/bin/clang++"))
    if not Path(compiler).is_file():
        compiler = shutil.which("clang++")
    first = toolchain.build(project, output, compiler, architecture="armv5t")
    second = toolchain.build(project, output, compiler, architecture="armv6")
    third = toolchain.build(project, output, compiler, architecture="armv5t")
    assert first["e32"]["architecture"] == "armv5t"
    assert second["e32"]["architecture"] == "armv6"
    assert second["elf"]["arm_attributes"]["cpu_arch"] == 6
    assert first["sha256"] != second["sha256"]
    assert first["sha256"] == third["sha256"]
    identity = json.loads((output / "cmake/symbian-toolchain.json").read_text())
    assert identity["architecture"] == "armv5t"
    with pytest.raises(StatusError):
        toolchain.build(
            project, tmp_path / "bad", compiler, architecture="armv7"
        )
    assert not (tmp_path / "bad").exists()


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_IDE_WORKSPACE"),
    reason="Set SYMBIAN_IDE_WORKSPACE for root host/guest compiler probes",
)
def test_root_file_api_compiler_information_retains_guest_target(tmp_path):
    import shlex
    import subprocess

    root = Path(os.environ["SYMBIAN_IDE_WORKSPACE"])
    reply = root / "build/debug/.cmake/api/v1/reply"
    for name in ("gui_app", "symbian_e32"):
        file = max(reply.glob(f"target-{name}-Debug-*.json"))
        groups = json.loads(file.read_text())["compileGroups"]
        for group in groups:
            if group["language"] != "CXX":
                continue
            flags = [
                flag
                for part in group["compileCommandFragments"]
                for flag in shlex.split(part["fragment"])
            ]
            command = [
                "/usr/bin/clang++",
                *flags,
                "-E",
                "-dM",
                "-x",
                "c++",
                "-",
            ]
            result = subprocess.run(
                command, input="", text=True, capture_output=True, timeout=20
            )
            (tmp_path / f"{name}.json").write_text(
                json.dumps({"command": command, "stderr": result.stderr})
            )
            assert result.returncode == 0, result.stderr
            assert ("__ARM_ARCH_6__" in result.stdout) == (name == "gui_app")
            assert ("--target=armv6-none-eabi" in flags) == (name == "gui_app")


def test_wrong_architecture_stops_before_guest_state_is_created(
    tmp_path, monkeypatch
):
    """Checks launch admission with explicit mocked format acceptance."""
    import hashlib
    from types import SimpleNamespace

    import symbian.emulator.launch as launch

    monkeypatch.setenv("XDG_CONFIG_HOME", str(tmp_path / "preferences"))
    root = tmp_path / "workspace"
    (root / "examples/gui_app").mkdir(parents=True)
    golden = tmp_path / "baseline"
    files = {}
    for dll in ("euser.dll", "ws32.dll", "gdi.dll"):
        relative = f"z/sys/bin/{dll}"
        path = golden / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"launch admission fixture, not a DLL parser test")
        files[relative] = hashlib.sha256(path.read_bytes()).hexdigest()
    emulator = tmp_path / "emulator"
    emulator.touch()
    device = SimpleNamespace(
        kernel="eka2", model="fixture", z_drive="z", rom="ROM", c_drive="c"
    )
    firmware = SimpleNamespace(device=device, files=files, identity="f" * 64)
    monkeypatch.setattr(launch, "selected", lambda _: (golden, firmware))
    monkeypatch.setattr(launch.toolchain, "build", lambda *args: {})
    monkeypatch.setattr(
        launch,
        "inspect_image",
        lambda _: {
            "architecture": "armv7",
            "uid3": 0xE0000811,
            "dll": False,
            "imports": [],
        },
    )
    with pytest.raises(StatusError) as failure:
        with launch.session(root, overrides={"emulator": emulator}):
            pytest.fail("Unsupported architecture started a guest")
    assert failure.value.code == Code.FAILED_PRECONDITION
    assert not (root / ".symbian/gui-runs").exists()
    assert not (root / ".symbian/gui-app").exists()


@pytest.mark.skipif(not TOOLS, reason="Clang tools required")
@pytest.mark.parametrize("architecture,cpu", [("armv5t", 3), ("armv6", 6)])
def test_object_probe_honors_target_selection(tmp_path, architecture, cpu):
    report = toolchain.probe(tmp_path, architecture=architecture)
    assert report["target"]["architecture"] == architecture
    assert report["elf"]["arm_attributes"]["cpu_arch"] == cpu
