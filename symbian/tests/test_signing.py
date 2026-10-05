"""Private identity management and real native SIS signing policy."""

import json
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian._native import build_sis, inspect_sis
from symbian.packaging.signing import IdentityStore, sign_package
from symbian.status import Code, StatusError
from symbian.tests.cli_json import main

pytestmark = pytest.mark.skipif(
    not shutil.which("openssl"), reason="OpenSSL required for identity checks"
)


@pytest.fixture(scope="module")
def image(tmp_path_factory):
    if not shutil.which("clang++") or not shutil.which("ld.lld"):
        pytest.skip("Clang and LLD required")
    project = Path(__file__).parents[2] / "probes/e32_probe"
    return Path(
        toolchain.build(project, tmp_path_factory.mktemp("signing-input"))[
            "artifact"
        ]
    )


@pytest.fixture
def store(tmp_path):
    return IdentityStore(tmp_path / "identities")


def test_create_import_list_and_archive_preserve_private_keys(store, tmp_path):
    first = store.create("developer", "Local Developer")
    certificate, key = store.paths("developer")
    key_bytes = key.read_bytes()
    assert first["fingerprint_sha256"]
    assert first["expires_at"]
    assert "Local Developer" in first["subject"]
    for path in (store.directory, certificate.parent):
        assert path.stat().st_mode & 0o777 == 0o700
    assert key.stat().st_mode & 0o777 == 0o600
    with pytest.raises(StatusError) as failure:
        store.create("developer", "Replacement")
    assert failure.value.code == Code.ALREADY_EXISTS
    assert key.read_bytes() == key_bytes
    imported = store.import_identity("imported", certificate, key)
    assert imported["fingerprint_sha256"] == first["fingerprint_sha256"]
    records = store.list()["identities"]
    assert [record["name"] for record in records] == ["developer", "imported"]
    assert "PRIVATE KEY" not in json.dumps(records)
    retired = store.archive("developer")
    assert (
        Path(retired["destination"]) / "signing.key"
    ).read_bytes() == key_bytes
    assert [item["name"] for item in store.list()["identities"]] == ["imported"]
    with pytest.raises(StatusError) as failure:
        store.paths("developer")
    assert failure.value.code == Code.NOT_FOUND


def test_invalid_import_and_names_publish_no_identity(store, tmp_path):
    store.create("first", "First")
    store.create("second", "Second")
    first_certificate, _ = store.paths("first")
    _, second_key = store.paths("second")
    with pytest.raises(StatusError) as failure:
        store.import_identity("mismatch", first_certificate, second_key)
    assert failure.value.code == Code.INVALID_ARGUMENT
    assert not (store.directory / "mismatch").exists()
    for name in ("../escape", ".hidden", "has space", "a" * 65):
        with pytest.raises(StatusError) as failure:
            store.create(name, "Invalid")
        assert failure.value.code == Code.INVALID_ARGUMENT
    assert not (tmp_path / "escape").exists()


def test_unsafe_identity_and_symlink_store_are_rejected(store, tmp_path):
    store.create("private", "Private")
    _, key = store.paths("private")
    key.chmod(0o644)
    with pytest.raises(StatusError) as failure:
        store.paths("private")
    assert failure.value.code == Code.FAILED_PRECONDITION
    link = tmp_path / "linked"
    link.symlink_to(store.directory, target_is_directory=True)
    with pytest.raises(StatusError) as failure:
        IdentityStore(link).create("elsewhere", "Elsewhere")
    assert failure.value.code == Code.FAILED_PRECONDITION
    assert not (store.directory / "elsewhere").exists()


def test_cli_identity_round_trip_uses_host_store(monkeypatch, tmp_path, capsys):
    monkeypatch.setenv("XDG_DATA_HOME", str(tmp_path))
    assert (
        main(
            [
                "signing",
                "create",
                "--identity",
                "cli",
                "--common-name",
                "CLI Developer",
            ]
        )
        == 0
    )
    record = json.loads(capsys.readouterr().out)["result"]
    assert record["name"] == "cli"
    assert main(["signing", "list"]) == 0
    listed = json.loads(capsys.readouterr().out)["result"]
    assert listed["identities"][0] == record
    assert main(["signing", "archive", "--identity", "cli"]) == 0
    assert json.loads(capsys.readouterr().out)["result"]["archived"]


def test_sign_existing_sis_verifies_and_preserves_inputs(
    store, image, tmp_path, capsys
):
    # Reuse the maintained real E32 fixture; the signer still performs native
    # whole-image/SIS validation rather than accepting synthetic package bytes.
    source = tmp_path / "application.sis"
    data = build_sis(
        image.read_bytes(), 0xE0000809, "Probe", "Research", "probe.exe"
    )
    source.write_bytes(data)
    store.create("developer", "Developer")
    certificate, key = store.paths("developer")
    destination = tmp_path / "signed.sis"
    assert (
        main(
            [
                "signing",
                "sign",
                str(source),
                "--identity",
                "developer",
                "--identities",
                str(store.directory),
                "--destination",
                str(destination),
            ]
        )
        == 0
    )
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["signed"] and result["sis"]["signed_package"]
    assert inspect_sis(destination.read_bytes()).signed_package
    assert source.read_bytes() == data
    before = destination.read_bytes()
    with pytest.raises(StatusError) as failure:
        sign_package(source, destination, certificate, key)
    assert failure.value.code == Code.ALREADY_EXISTS
    assert destination.read_bytes() == before
    with pytest.raises(StatusError):
        sign_package(source, source, certificate, key)
    assert source.read_bytes() == data
    invalid = tmp_path / "invalid.sis"
    invalid.write_bytes(b"not a SIS")
    with pytest.raises(StatusError):
        sign_package(invalid, tmp_path / "absent.sis", certificate, key)
    assert not (tmp_path / "absent.sis").exists()
