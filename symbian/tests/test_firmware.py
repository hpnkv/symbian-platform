"""Preference provenance, portable firmware and owned import failures."""

import json
import os
import subprocess
import sys
from pathlib import Path

import pytest

from symbian.emulator.configuration import (
    atomic_json,
    configure,
    resolve,
)
from symbian.emulator.firmware import (
    Device,
    Manifest,
    describe,
    digest,
    export_firmware,
    identity,
    import_firmware,
    inventory,
    locate,
    selected,
    svc_profile,
    validate_manifest,
)
from symbian.paths import asset_directory
from symbian.status import Code, StatusError
from symbian.tests.cli_json import main


@pytest.fixture(autouse=True)
def isolated_preferences(tmp_path, monkeypatch):
    for kind in ("CONFIG", "DATA", "CACHE"):
        monkeypatch.setenv(f"XDG_{kind}_HOME", str(tmp_path / kind.lower()))
    monkeypatch.delenv("SYMBIAN_SDK_MANIFEST", raising=False)
    monkeypatch.chdir(tmp_path)


def bundle(tmp_path):
    directory = tmp_path / "bundle"
    device = Device(
        firmware_code="RM-675",
        model="C7-00",
        manufacturer="Nokia",
        symbian_version="epoc100",
        kernel="eka2",
        machine_uid=0,
        rom="data/roms/rm-675/SYM.ROM",
        z_drive="data/drives/z/rm-675",
        c_drive="data/drives/rm-675/c",
    )
    for path in (device.rom, device.z_drive + "/sys/bin/euser.dll"):
        target = directory / "instance" / path
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(path.encode())
    files = inventory(directory / "instance")
    manifest = Manifest(
        identity=identity(device, files),
        device=device,
        files=files,
        provenance={"test": "store policy; not a ROM parser test"},
    )
    atomic_json(directory / "firmware.json", manifest.model_dump(mode="json"))
    return directory, manifest


def test_precedence_provenance_and_relative_paths(tmp_path):
    sdk = tmp_path / "SDK"
    sdk.mkdir()
    project = tmp_path / "project"
    project.mkdir()
    configure(
        "global",
        {"firmware": "global", "backend": "dyncom", "store": "relative-store"},
    )
    configure("sdk", {"firmware": "sdk", "emulator": "bin/emulator"}, sdk=sdk)
    configure("project", {"firmware": "project"}, project=project)
    resolution = resolve(
        project=project, sdk=sdk, overrides={"firmware": "command"}
    )
    assert resolution.settings.firmware == "command"
    assert resolution.settings.backend == "dyncom"
    assert resolution.settings.store == asset_directory("config") / "relative-store"
    assert resolution.settings.emulator == sdk / "bin/emulator"
    assert resolution.origins["firmware"] == "command"
    assert resolution.origins["emulator"] == str(sdk / "emulator.json")
    assert len(resolution.layers) == 3
    assert resolve(project=project, sdk=sdk).settings.firmware == "project"
    configure("project", {}, unset=["firmware"], project=project)
    assert resolve(project=project, sdk=sdk).settings.firmware == "sdk"
    configure("project", {"firmware": None}, project=project)
    assert resolve(project=project, sdk=sdk).settings.firmware is None


def test_project_sdk_selector_is_used_without_loading_build_dependencies(
    tmp_path,
):
    sdk = tmp_path / "SDK"
    sdk.mkdir()
    configure("sdk", {"firmware": "selected-sdk"}, sdk=sdk)
    project = tmp_path / "project"
    project.mkdir()
    atomic_json(project / "sdk-location.json", {"sdk": "../SDK"})
    atomic_json(
        project / "symbian-project.json",
        {"preferences": {"name": "test", "uid3": 0xE0000801}},
    )
    resolution = resolve(project=project)
    assert resolution.sdk == sdk
    assert resolution.settings.firmware == "selected-sdk"


def test_invalid_configuration_never_falls_back(tmp_path):
    configure("global", {"firmware": "valid"})
    sdk = tmp_path / "SDK"
    sdk.mkdir()
    (sdk / "emulator.json").write_text('{"firmware": 42}')
    with pytest.raises(StatusError) as failure:
        resolve(sdk=sdk, overrides={"firmware": "command"})
    assert failure.value.code == Code.INVALID_ARGUMENT
    assert str(sdk / "emulator.json") in str(failure.value)


def test_no_default_firmware_guess_or_hidden_808(tmp_path):
    resolution = resolve()
    assert resolution.settings.store == asset_directory("data") / "firmware"
    assert resolution.settings.firmware is None
    explanation = describe(resolution)
    assert explanation["selection_status"]["code"] == Code.FAILED_PRECONDITION
    assert not asset_directory("data").exists()


def test_portable_bundle_moves_between_stores_and_aliases(tmp_path):
    source, manifest = bundle(tmp_path)
    first = resolve(overrides={"store": tmp_path / "store-a"})
    imported = import_firmware(
        first, source=source, form="bundle", name="first"
    )
    reference = f"sha256:{manifest.identity}"
    assert imported["firmware"] == reference
    exported = tmp_path / "exported"
    export_firmware(first.settings.store, "first", exported)
    second = resolve(
        overrides={"store": tmp_path / "store-b", "firmware": reference}
    )
    import_firmware(second, source=exported, form="bundle", name="second")
    instance, verified = selected(second)
    assert instance.is_relative_to(second.settings.store)
    assert verified.identity == manifest.identity
    assert locate(second.settings.store, "second") == instance.parent
    assert (
        not (second.settings.store / "aliases.json").read_text().find("first")
        >= 0
    )


def test_tampered_or_extra_baseline_files_are_not_launched(tmp_path):
    source, _ = bundle(tmp_path)
    resolution = resolve(overrides={"firmware": "test"})
    imported = import_firmware(
        resolution, source=source, form="bundle", name="test"
    )
    directory = Path(imported["directory"])
    (directory / "instance/extra").write_text("unexpected")
    with pytest.raises(StatusError) as failure:
        selected(resolution)
    assert failure.value.code == Code.DATA_LOSS


def test_profile_is_not_applied_to_unrelated_device(tmp_path):
    _, manifest = bundle(tmp_path)
    assert svc_profile(manifest, "auto") == "default"
    with pytest.raises(StatusError) as failure:
        svc_profile(manifest, "rm807-113.010.1508")
    assert failure.value.code == Code.FAILED_PRECONDITION


def test_alias_replacement_is_explicit(tmp_path):
    source, _ = bundle(tmp_path)
    resolution = resolve()
    first = import_firmware(
        resolution, source=source, form="bundle", name="device"
    )
    manifest = validate_manifest(source)
    manifest.device.model = "Different metadata"
    manifest.identity = identity(manifest.device, manifest.files)
    atomic_json(source / "firmware.json", manifest.model_dump(mode="json"))
    with pytest.raises(StatusError) as failure:
        import_firmware(resolution, source=source, form="bundle", name="device")
    assert failure.value.code == Code.ALREADY_EXISTS
    assert locate(resolution.settings.store, "device") == Path(
        first["directory"]
    )
    changed = import_firmware(
        resolution,
        source=source,
        form="bundle",
        name="device",
        replace_alias=True,
    )
    assert changed["firmware"] != first["firmware"]


def test_bundle_rejects_symlink(tmp_path):
    source, _ = bundle(tmp_path)
    (source / "instance/link").symlink_to(tmp_path)
    with pytest.raises(StatusError) as failure:
        import_firmware(resolve(), source=source, form="bundle")
    assert failure.value.code == Code.INVALID_ARGUMENT


def test_native_timeout_is_reaped_and_evidence_retained(tmp_path):
    executable = tmp_path / "slow-importer"
    executable.write_text(
        f"#!{sys.executable}\nimport os,time\nfrom pathlib import"
        " Path\nPath('pid').write_text(str(os.getpid()))\ntime.sleep(60)\n"
    )
    executable.chmod(0o755)
    archive = tmp_path / "source"
    archive.write_text("unchanged input")
    resolution = resolve(overrides={"importer": executable})
    with pytest.raises(StatusError) as failure:
        import_firmware(resolution, source=archive, timeout=1.5)
    assert failure.value.code == Code.DEADLINE_EXCEEDED
    retained = next((asset_directory("cache") / "firmware-imports").glob("import-*"))
    with pytest.raises(ProcessLookupError):
        os.kill(int((retained / "instance/pid").read_text()), 0)
    assert (retained / "failure.json").is_file()
    assert archive.read_text() == "unchanged input"


def test_cli_selection_explains_missing_firmware(capsys):
    assert main(["emu", "resolve", "--firmware", "missing"]) == 0
    response = json.loads(capsys.readouterr().out)
    assert response["result"]["origins"]["firmware"] == "command"
    assert response["result"]["selection_status"]["code"] == Code.NOT_FOUND


def test_guest_outcome_reports_errors_even_with_successful_frontend(tmp_path):
    from symbian.emulator.launch import guest_outcome

    report = tmp_path / "exit.json"
    atomic_json(
        report,
        {
            "result": {
                "process_exits": [
                    {"uid": 123, "name": "app", "type": 0, "reason": -5}
                ]
            }
        },
    )
    with pytest.raises(StatusError) as failure:
        guest_outcome(report, 123)
    assert failure.value.code == Code.UNIMPLEMENTED
    atomic_json(
        report,
        {
            "result": {
                "process_exits": [
                    {"uid": 123, "name": "app", "type": 0, "reason": 0}
                ]
            }
        },
    )
    guest_outcome(report, 123)
    with pytest.raises(StatusError) as failure:
        guest_outcome(report, 124)
    assert failure.value.code == Code.DATA_LOSS


IMPORTER = os.environ.get("SYMBIAN_FIRMWARE_IMPORTER")
DUMPS = os.environ.get("SYMBIAN_ROM_DUMPS")
SDK = os.environ.get("SYMBIAN_APP_SDK")
live = pytest.mark.skipif(
    not IMPORTER or not DUMPS,
    reason="Set SYMBIAN_FIRMWARE_IMPORTER and SYMBIAN_ROM_DUMPS",
)


@live
@pytest.mark.parametrize(
    "entries",
    [
        {"../escape": b"bad"},
        {"/escape": b"bad"},
        {"A.rom": b"one", "B.rom": b"two"},
        {"data/file": b"one", "data/File": b"two"},
    ],
)
def test_native_archive_rejection_never_publishes(tmp_path, entries):
    import zipfile

    archive = tmp_path / "bad.zip"
    with zipfile.ZipFile(archive, "w") as stream:
        for path, content in entries.items():
            stream.writestr(path, content)
    resolution = resolve(overrides={"importer": Path(IMPORTER)})
    with pytest.raises(StatusError) as failure:
        import_firmware(resolution, source=archive, name="bad")
    assert failure.value.code == Code.INVALID_ARGUMENT
    assert not (tmp_path / "escape").exists()
    assert not (resolution.settings.store / "objects").exists()


@live
@pytest.mark.parametrize(
    "pattern,kernel",
    [
        ("C7-00*.7z", "eka2"),
        ("E6-00*.7z", "eka2"),
        ("6120*.7z", "eka2"),
        ("E71*.7z", "eka2"),
        ("7610 RH-51*.7z", "eka1"),
        ("P900*.7z", "eka1"),
    ],
)
def test_original_native_import_forms_are_portable(tmp_path, pattern, kernel):
    source = next(Path(DUMPS).rglob(pattern))
    before = digest(source)
    resolution = resolve(
        overrides={"importer": Path(IMPORTER), "firmware": "device"}
    )
    imported = import_firmware(resolution, source=source, name="device")
    _, manifest = selected(resolution)
    assert manifest.device.kernel == kernel
    assert digest(source) == before
    assert imported["firmware"] == f"sha256:{manifest.identity}"
    if kernel == "eka1":
        from symbian.emulator.launch import session

        (tmp_path / "examples/gui_app").mkdir(parents=True)
        with pytest.raises(StatusError) as failure:
            with session(
                tmp_path,
                overrides={
                    "store": resolution.settings.store,
                    "firmware": "device",
                },
            ):
                pytest.fail("EKA1 accepted the unsupported EKA2 starter ABI")
        assert failure.value.code == Code.FAILED_PRECONDITION
        assert "EKA1" in str(failure.value)
        assert not (tmp_path / ".symbian/gui-runs").exists()


@live
@pytest.mark.skipif(not SDK, reason="Set SYMBIAN_APP_SDK")
@pytest.mark.parametrize(
    "pattern", ["C7-00*.7z", "E6-00*.7z", "6120*.7z", "E71*.7z"]
)
def test_init_build_and_real_execution_for_other_devices(tmp_path, pattern):
    import time

    from PIL import Image

    from symbian.emulator import Control
    from symbian.emulator.launch import session
    from symbian.tests.test_guest_gui import _ready

    resolution = resolve(sdk=Path(SDK), overrides={"importer": Path(IMPORTER)})
    imported = import_firmware(
        resolution, source=next(Path(DUMPS).rglob(pattern)), name="device"
    )
    project = tmp_path / "application"
    result = subprocess.run(
        [
            sys.executable,
            "-m",
            "symbian.cli",
            "--output-format=json",
            "init",
            str(project),
            "--sdk",
            SDK,
            "--name",
            "device_starter",
            "--ide",
            "none",
            "--non-interactive",
            "--firmware",
            "device",
            "--store",
            str(resolution.settings.store),
        ],
        text=True,
        capture_output=True,
        timeout=90,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
    assert json.loads(result.stdout)["result"]["initial_build"] is True
    assert (
        json.loads((project / "emulator.json").read_text())["firmware"]
        == imported["firmware"]
    )
    with session(project, project=project) as active:
        control = Control(active.endpoint)
        attempt = 0

        def captured(label):
            nonlocal attempt
            attempt += 1
            metadata = _ready(lambda: control.capture(f"{label}-{attempt}"))
            return metadata, Image.open(metadata["path"]).convert("RGB")

        deadline = time.monotonic() + 15
        while True:
            capture, initial = captured("before")
            scale = capture["display_scale"]
            greeting = initial.crop(
                (int(12 * scale), 0, int(170 * scale), int(48 * scale))
            )
            if (
                sum(min(pixel) > 150 for pixel in greeting.get_flattened_data())
                > 100
            ):
                break
            assert time.monotonic() < deadline
            time.sleep(0.05)
        initial.save(tmp_path / "before.png")
        # Render/input use actual Window Server logical pixels, not the
        # 808's fixed dimensions. Capture is scaled by frontend display scale.
        height = capture["logical_height"]
        width = capture["logical_width"]
        _ready(lambda: control.pointer(width // 2, height // 3, "press"))
        _ready(lambda: control.pointer(width // 2, height // 3, "release"))
        deadline = time.monotonic() + 15
        while True:
            _, logged = captured("logged")
            first_line = 80 if height < 480 else 136
            row = logged.crop(
                (
                    int(12 * scale),
                    int((first_line - 20) * scale),
                    int((width - 12) * scale),
                    int((first_line + 1) * scale),
                )
            )
            if (
                sum(min(pixel) > 150 for pixel in row.get_flattened_data())
                > 100
            ):
                break
            assert time.monotonic() < deadline
            time.sleep(0.05)
        logged.save(tmp_path / "logged.png")
        _ready(lambda: control.pointer(width - 30, height - 15, "press"))
        assert active.process.wait(timeout=15) == 0
    report = json.loads((active.directory / "launch.json").read_text())
    assert report["inputs_unchanged"] is True
    assert report["svc_profile"] == "default"
    from symbian.emulator.launch import guest_outcome

    guest_outcome(
        active.directory / "control.sock.status.json", active.image["uid3"]
    )
