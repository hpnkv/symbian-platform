"""Installation checks preserve the selection and reject incompatible input."""

import io
import json
import tarfile
from pathlib import Path

import httpx
import pytest

from symbian.emulator import distribution as emulator
from symbian.emulator.configuration import configure, resolve
from symbian.status import StatusError
from symbian.tests.cli_json import main


@pytest.fixture(autouse=True)
def preferences(tmp_path, monkeypatch):
    for kind in ("CONFIG", "DATA", "CACHE"):
        monkeypatch.setenv(f"XDG_{kind}_HOME", str(tmp_path / kind.lower()))
    monkeypatch.delenv("SYMBIAN_SDK_MANIFEST", raising=False)
    monkeypatch.chdir(tmp_path)


def archive(tmp_path, version="0.1.0", *, reported=None, system=None):
    prefix = tmp_path / f"input-{version}"
    prefix.mkdir()
    (prefix / "bin").mkdir()
    (prefix / "licenses").mkdir()
    (prefix / "licenses/COPYING").write_text("Fixture license\n")
    host_system, arch = emulator.host()
    metadata = {
        "schema": "symbian.emulator-distribution/v1",
        "version": version,
        "system": system or host_system,
        "architecture": arch,
        "frontend": "bin/frontend",
        "importer": "bin/importer",
        "control_protocol": emulator.PROTOCOL,
        "capabilities": sorted(emulator.REQUIRED),
    }
    (prefix / "emulator.json").write_text(json.dumps(metadata))
    response = {
        "schema": "symbian.emulator-capabilities/v1",
        "version": reported or version,
        "control_protocol": emulator.PROTOCOL,
        "capabilities": sorted(emulator.REQUIRED),
    }
    (prefix / "bin/frontend").write_text(
        "#!/bin/sh\nprintf '%s\\n' '" + json.dumps(response) + "'\n"
    )
    (prefix / "bin/importer").write_text("#!/bin/sh\nexit 0\n")
    for path in (prefix / "bin").iterdir():
        path.chmod(0o755)
    result = tmp_path / f"emulator-{version}.tar.gz"
    with tarfile.open(result, "w:gz") as bundle:
        bundle.add(prefix, arcname=".")
    return result


def test_install_select_rollback_and_precedence(tmp_path):
    first = emulator.install(archive=archive(tmp_path))
    assert first["version"] == "0.1.0"
    old = Path(first["prefix"])
    assert resolve().settings.emulator == old / "bin/frontend"
    emulator.install(archive=archive(tmp_path, "0.2.0"))
    assert old.is_dir()
    assert emulator.select("0.1.0")["version"] == "0.1.0"
    configure("global", {"emulator": "/explicit/frontend"})
    assert resolve().settings.emulator == Path("/explicit/frontend")
    assert resolve(
        overrides={"emulator": "/command/frontend"}
    ).settings.emulator == Path("/command/frontend")
    assert len(emulator.installed()["installations"]) == 2


def test_incompatible_executable_keeps_previous_selection(tmp_path):
    emulator.install(archive=archive(tmp_path))
    previous = emulator.active_path().read_bytes()
    bad = archive(tmp_path, "0.2.0", reported="0.3.0")
    with pytest.raises(StatusError, match="version differs"):
        emulator.install(archive=bad)
    assert emulator.active_path().read_bytes() == previous
    assert len(emulator.installed()["installations"]) == 1
    assert not list(emulator.root().glob(".install-*"))


def test_wrong_host_rejected_before_executing(tmp_path, monkeypatch):
    system = "macos" if emulator.host()[0] == "linux" else "linux"
    bad = archive(tmp_path, system=system)
    monkeypatch.setattr(
        emulator,
        "query",
        lambda path: pytest.fail("Executed wrong-host binary"),
    )
    with pytest.raises(StatusError, match="another host"):
        emulator.install(archive=bad)
    assert not emulator.active_path().exists()


def test_traversal_archive_does_not_change_selected_installation(tmp_path):
    emulator.install(archive=archive(tmp_path))
    previous = emulator.active_path().read_bytes()
    bad = tmp_path / "bad.tar.gz"
    with tarfile.open(bad, "w:gz") as bundle:
        member = tarfile.TarInfo("../../escape")
        member.size = 1
        bundle.addfile(member, io.BytesIO(b"x"))
    with pytest.raises(StatusError, match="Invalid emulator archive"):
        emulator.install(archive=bad)
    assert emulator.active_path().read_bytes() == previous
    assert not (tmp_path / "escape").exists()


def test_digest_and_requested_version_are_checked(tmp_path):
    bundle = archive(tmp_path)
    with pytest.raises(StatusError, match="digest differs"):
        emulator.install(archive=bundle, sha256="0" * 64)
    with pytest.raises(StatusError, match="requested emulator version"):
        emulator.install(archive=bundle, version="0.2.0")
    assert not emulator.active_path().exists()


def test_sdk_upgrade_reuses_selected_compatible_emulator(tmp_path, monkeypatch):
    emulator.install(archive=archive(tmp_path, "0.1.0"))
    monkeypatch.setattr(
        emulator,
        "released",
        lambda version: pytest.fail("Redownloaded compatible emulator"),
    )
    result = emulator.install()
    assert result["reused"] and result["version"] == "0.1.0"


def test_cli_alias_doctor_and_contract_failure(tmp_path, capsys):
    assert (
        main(["emulator", "install", "--archive", str(archive(tmp_path))]) == 0
    )
    capsys.readouterr()
    assert main(["emulator", "doctor"]) == 0
    response = json.loads(capsys.readouterr().out)
    assert response["result"]["control_protocol"] == emulator.PROTOCOL
    assert main(["emu", "list"]) == 0
    capsys.readouterr()
    with pytest.raises(StatusError, match="missing capabilities"):
        emulator.compatible(emulator.PROTOCOL, [])


def test_discovery_uses_highest_compatible_independent_release(monkeypatch):
    system, arch = emulator.host()

    def release(version):
        return {
            "tag_name": f"emulator-v{version}",
            "draft": False,
            "prerelease": False,
            "assets": [
                {
                    "name": (
                        f"symbian-emulator-{version}-{system}-{arch}.tar.gz"
                    ),
                    "browser_download_url": f"https://assets.test/{version}.tar.gz",
                },
                {
                    "name": f"symbian-emulator-{version}.json",
                    "browser_download_url": f"https://assets.test/{version}.json",
                },
            ],
        }

    def respond(request):
        if request.url.host == "api.github.com":
            return httpx.Response(
                200,
                json=[release("0.9.0"), release("0.10.0"), release("1.0.0")],
            )
        version = request.url.path[1:].removesuffix(".json")
        return httpx.Response(
            200,
            json={
                "schema": "symbian.emulator-release/v1",
                "version": version,
                "control_protocol": emulator.PROTOCOL,
                "capabilities": (
                    [] if version == "1.0.0" else sorted(emulator.REQUIRED)
                ),
            },
        )

    client = httpx.Client
    monkeypatch.setattr(
        emulator.httpx,
        "Client",
        lambda **kwargs: client(
            transport=httpx.MockTransport(respond), **kwargs
        ),
    )
    assert "0.10.0" in emulator.released(None)["name"]
    assert "0.9.0" in emulator.released("0.9.0")["name"]
    with pytest.raises(StatusError, match="No matching published"):
        emulator.released("1.0.0")


def test_packaged_frontend_rechecked_before_launch(tmp_path):
    installation = emulator.install(archive=archive(tmp_path))
    frontend = Path(installation["prefix"]) / "bin/frontend"
    emulator.check_packaged(frontend)
    frontend.write_text("#!/bin/sh\necho '{}'\n")
    with pytest.raises(StatusError, match="compatibility check failed"):
        emulator.check_packaged(frontend)
