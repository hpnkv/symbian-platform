"""Standalone emulator ownership and optional application staging."""

import json
import os
import sys
from pathlib import Path

import pytest

from symbian.cli.__main__ import _execute, _parser
from symbian.emulator import launch
from symbian.emulator.configuration import Resolution, Settings
from symbian.emulator.firmware import Device, Manifest, digest


def _frontend(monkeypatch, tmp_path, kernel="eka2"):
    golden = tmp_path / "golden"
    golden.mkdir()
    rom = golden / "rom.bin"
    rom.write_bytes(b"baseline")
    device = Device(
        firmware_code="test",
        model="Test",
        manufacturer="Test",
        symbian_version="test",
        kernel=kernel,
        machine_uid=1,
        rom="rom.bin",
        z_drive="data/z",
        c_drive="data/c",
    )
    firmware = Manifest(
        identity="test",
        device=device,
        files={"rom.bin": digest(rom)},
        provenance={},
    )
    executable = tmp_path / "emulator.py"
    executable.write_text(
        f"#!{sys.executable}\n"
        "import json, os, sys, time\n"
        "from pathlib import Path\n"
        "Path('child.json').write_text(json.dumps({"
        "'argv': sys.argv[1:], 'root': os.environ['EKA2L1_DATA_ROOT']}))\n"
        "time.sleep(60)\n"
    )
    executable.chmod(0o700)
    resolution = Resolution(
        settings=Settings(emulator=executable), origins={}, layers=[]
    )
    monkeypatch.setattr(launch, "resolve", lambda **kwargs: resolution)
    monkeypatch.setattr(launch, "selected", lambda _: (golden, firmware))
    monkeypatch.setattr(launch, "svc_profile", lambda *_: "default")
    monkeypatch.setattr(launch, "executable_for_session", lambda path, _: path)

    return golden, firmware


@pytest.mark.parametrize("kernel", ["eka1", "eka2"])
def test_standalone_owns_child_and_preserves_baseline(
    monkeypatch, tmp_path, kernel
):
    golden, _ = _frontend(monkeypatch, tmp_path, kernel)
    rom = golden / "rom.bin"

    def forbidden(*args, **kwargs):
        pytest.fail("Standalone launch touched application tools")

    monkeypatch.setattr(launch.toolchain, "build", forbidden)
    monkeypatch.setattr(launch, "llvm_tool", forbidden)
    monkeypatch.setattr(launch, "inspect_image", forbidden)
    monkeypatch.setattr(
        launch.Session, "wait_ready", lambda *_args, **_kwargs: None
    )
    with launch.session(tmp_path, standalone=True) as active:
        child = active.process
        child_path = active.directory / "child.json"
        import time

        deadline = time.monotonic() + 5
        while not child_path.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        command = json.loads(child_path.read_text())
        assert command["argv"] == ["--device", "test"]
        copied = Path(command["root"])
        assert copied != golden
        assert (copied / "rom.bin").read_bytes() == b"baseline"
        assert not (copied / "data/c/sys/bin/gui_app.exe").exists()
        endpoint = active.endpoint
        assert active.image == {} and active.symbols is None
    assert child.poll() is not None
    with pytest.raises(ProcessLookupError):
        os.kill(child.pid, 0)
    assert not endpoint.parent.exists()
    record = json.loads((active.directory / "launch.json").read_text())
    assert record["standalone"] and record["inputs_unchanged"]
    assert record["e32_sha256"] is None and record["elf_sha256"] is None
    assert record["frontend_exit"] is not None
    assert rom.read_bytes() == b"baseline"


def test_cli_standalone_forwards_selection_and_skips_guest_outcome(
    monkeypatch, tmp_path
):
    observed = []
    monkeypatch.setattr(
        launch, "main", lambda argv, **kwargs: observed.append(argv) or 0
    )
    args = _parser().parse_args(
        [
            "emu",
            "run",
            "--root",
            str(tmp_path),
            "--firmware",
            "chosen",
            "--backend",
            "dyncom",
            "--sdk",
            str(tmp_path / "sdk"),
        ]
    )
    assert _execute(args) == {"frontend_exit": 0, "standalone": True}
    assert observed == [
        [
            "--standalone",
            "--root",
            str(tmp_path),
            "--sdk",
            str(tmp_path / "sdk"),
            "--firmware",
            "chosen",
            "--backend",
            "dyncom",
        ]
    ]


def test_standalone_supervisor_does_not_require_application_exit(
    monkeypatch, tmp_path
):
    """Accept frontend close without a guest application exit report."""
    from contextlib import contextmanager
    from types import SimpleNamespace

    @contextmanager
    def fake_session(root, **kwargs):
        assert kwargs["standalone"] is True
        yield SimpleNamespace(
            directory=tmp_path,
            process=SimpleNamespace(wait=lambda: 0),
            image={},
        )

    def forbidden(*args, **kwargs):
        pytest.fail("Standalone supervisor required a guest application exit")

    monkeypatch.setattr(launch, "session", fake_session)
    monkeypatch.setattr(launch, "guest_outcome", forbidden)
    monkeypatch.setattr(launch.signal, "signal", lambda *_: None)
    assert launch.main(["--standalone", "--root", str(tmp_path)]) == 0


@pytest.mark.parametrize(
    "standalone,implicit", [(True, False), (True, True), (False, False)]
)
@pytest.mark.parametrize("registered", [False, True])
def test_launch_stages_application_resources(
    monkeypatch, tmp_path, standalone, implicit, registered
):
    """Automatic and menu launches receive the same registration resources."""
    import time
    from types import SimpleNamespace

    from symbian.packaging import registration
    from symbian.project import sdk

    golden, _ = _frontend(monkeypatch, tmp_path)
    project = tmp_path if implicit else tmp_path / "app"
    project.mkdir(exist_ok=True)
    manifest = '[project]\nname="demo"\nuid3=3758098449\n'
    if registered:
        manifest += '[package]\nname="Demo"\n[application]\ncaption="Demo"\n'
    (project / "symbian.toml").write_text(manifest)
    for dll in ("euser.dll", "ws32.dll", "gdi.dll"):
        path = golden / "data/z/sys/bin" / dll
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes(b"firmware DLL")
    selected_sdk = SimpleNamespace(
        prefix=tmp_path / "sdk", compiler=Path("clang++"), linker=Path("ld.lld")
    )
    monkeypatch.setattr(sdk, "discover_sdk", lambda *_: tmp_path / "sdk.json")
    monkeypatch.setattr(sdk.AppSdk, "load", lambda _: selected_sdk)
    builds = []

    def build(source, output, compiler, linker):
        builds.append(source)
        output.mkdir(parents=True, exist_ok=True)
        (output / "demo.exe").write_bytes(b"compiled E32")
        (output / "demo.elf").write_bytes(b"compiled ELF")

    def resources(source, application, name, uid, **kwargs):
        assert source == project and name == "demo.exe"
        assert kwargs["sdk"] is selected_sdk
        return [
            (
                "!:\\private\\10003a3f\\import\\apps\\demo_reg.rsc",
                b"compiled registration",
            )
        ], {}

    monkeypatch.setattr(launch.toolchain, "build", build)
    monkeypatch.setattr(registration, "compile_registration", resources)
    monkeypatch.setattr(
        launch,
        "inspect_image",
        lambda _: {
            "uid3": 3758098449,
            "dll": False,
            "imports": [],
            "architecture": "armv6",
        },
    )
    monkeypatch.setattr(launch.Session, "wait_ready", lambda *_, **kwargs: None)
    with launch.session(
        tmp_path, project=None if implicit else project, standalone=standalone
    ) as active:
        child_path = active.directory / "child.json"
        deadline = time.monotonic() + 5
        while not child_path.exists() and time.monotonic() < deadline:
            time.sleep(0.01)
        command = json.loads(child_path.read_text())
        assert builds == [project]
        assert command["argv"] == [
            "--device",
            "test",
            *([] if standalone else ["--run", "C:\\sys\\bin\\demo.exe"]),
        ]
        target = Path(command["root"]) / "data/c/sys/bin/demo.exe"
        assert target.read_bytes() == b"compiled E32"
        resource = (
            Path(command["root"])
            / "data/c/private/10003a3f/import/apps/demo_reg.rsc"
        )
        if registered:
            assert resource.read_bytes() == b"compiled registration"
        else:
            assert not resource.exists()
        assert active.image["uid3"] == 3758098449
    saved = json.loads((active.directory / "launch.json").read_text())
    assert saved["application"]["project"] == str(project)
    assert saved["application"]["auto_run"] is not standalone
    assert saved["e32_sha256"] == digest(project / ".symbian/build/demo.exe")
    assert saved["elf_sha256"] == digest(project / ".symbian/build/demo.elf")
    assert saved["inputs_unchanged"]
    assert not (golden / "data/c/sys/bin/demo.exe").exists()
