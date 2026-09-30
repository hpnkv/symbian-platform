"""Preservation must detect missing, altered, extra and redirected artifacts."""

import hashlib
import json
import os
from pathlib import Path

import pytest

from symbian import preservation
from symbian.status import Code, StatusError


@pytest.fixture
def archive(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    (source / "firmware.bin").write_bytes(b"original firmware fixture\0")
    (source / "empty.bin").write_bytes(b"")
    (source / "sub").mkdir()
    (source / "sub" / "Z.bin").write_bytes(b"ROM fixture")
    destination = tmp_path / "archive"
    result = preservation.create(source, destination, "test fixture")
    yield source, destination, result
    for parent, _directories, filenames in os.walk(destination):
        Path(parent).chmod(0o755)
        for filename in filenames:
            path = Path(parent) / filename
            if not path.is_symlink():
                path.chmod(0o644)


def test_sealed_verified_copy_and_trusted_manifest(archive):
    source, destination, result = archive
    assert result["file_count"] == 3
    assert result["trusted_manifest_checked"]
    assert not (destination.stat().st_mode & 0o222)
    assert not ((destination / "artifacts/firmware.bin").stat().st_mode & 0o222)
    assert preservation.verify(destination, result["manifest_sha256"])[
        "verified"
    ]
    (source / "firmware.bin").write_bytes(b"later source change")
    assert preservation.verify(destination)["verified"]


def test_creation_never_overwrites_archive(archive):
    source, destination, result = archive
    with pytest.raises(StatusError) as caught:
        preservation.create(source, destination)
    assert caught.value.code == Code.ALREADY_EXISTS
    assert preservation.verify(destination, result["manifest_sha256"])[
        "verified"
    ]


def test_changed_bytes_are_detected(archive):
    _, destination, _ = archive
    artifact = destination / "artifacts/firmware.bin"
    artifact.chmod(0o644)
    artifact.write_bytes(b"tampered")
    with pytest.raises(StatusError) as caught:
        preservation.verify(destination)
    assert caught.value.code == Code.DATA_LOSS


def test_manifest_substitution_is_detected_by_trusted_hash(archive):
    _, destination, result = archive
    manifest = destination / "manifest.json"
    manifest.chmod(0o644)
    manifest.write_bytes(manifest.read_bytes() + b" ")
    with pytest.raises(StatusError) as caught:
        preservation.verify(destination, result["manifest_sha256"])
    assert caught.value.code == Code.DATA_LOSS


def test_extra_file_is_detected(archive):
    _, destination, _ = archive
    destination.chmod(0o755)
    (destination / "extra.bin").write_bytes(b"extra")
    with pytest.raises(StatusError) as caught:
        preservation.verify(destination)
    assert caught.value.code == Code.DATA_LOSS


def test_missing_file_is_detected(archive):
    _, destination, _ = archive
    (destination / "artifacts").chmod(0o755)
    (destination / "artifacts/empty.bin").unlink()
    with pytest.raises(StatusError) as caught:
        preservation.verify(destination)
    assert caught.value.code == Code.DATA_LOSS


@pytest.mark.parametrize(
    "name", ["../outside", "/tmp/outside", "artifacts/../x", "artifacts/\0x"]
)
def test_unsafe_manifest_path_is_rejected(archive, name):
    _, destination, _ = archive
    path = destination / "manifest.json"
    body = json.loads(path.read_bytes())
    body["files"][0]["path"] = name
    path.chmod(0o644)
    path.write_text(json.dumps(body))
    with pytest.raises(StatusError) as caught:
        preservation.verify(destination)
    assert caught.value.code == Code.DATA_LOSS


def test_input_symlink_is_rejected(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    (source / "link").symlink_to(tmp_path / "outside")
    with pytest.raises(StatusError):
        preservation.create(source, tmp_path / "archive")
    assert not (tmp_path / "archive").exists()


def test_linked_directory_is_rejected(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    outside = tmp_path / "outside"
    outside.mkdir()
    (source / "directory").symlink_to(outside, target_is_directory=True)
    with pytest.raises(StatusError):
        preservation.create(source, tmp_path / "archive")


def test_fifo_is_rejected_without_blocking(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    os.mkfifo(source / "fifo")
    with pytest.raises(StatusError):
        preservation.create(source, tmp_path / "archive")


def test_archive_cannot_be_within_source(tmp_path):
    (tmp_path / "firmware").write_bytes(b"fixture")
    with pytest.raises(StatusError):
        preservation.create(tmp_path, tmp_path / "archive")


def test_incomplete_archive_cannot_verify(tmp_path):
    (tmp_path / ".incomplete").write_text("interrupted")
    with pytest.raises(StatusError) as caught:
        preservation.verify(tmp_path)
    assert caught.value.code == Code.FAILED_PRECONDITION


def test_duplicate_manifest_records_are_rejected(archive):
    _, destination, _ = archive
    path = destination / "manifest.json"
    body = json.loads(path.read_bytes())
    body["files"].append(body["files"][0])
    path.chmod(0o644)
    path.write_text(json.dumps(body))
    with pytest.raises(StatusError) as caught:
        preservation.verify(destination)
    assert caught.value.code == Code.DATA_LOSS


def test_archive_link_to_outside_is_rejected(archive):
    source, destination, _ = archive
    artifact = destination / "artifacts/firmware.bin"
    artifact.parent.chmod(0o755)
    artifact.unlink()
    artifact.symlink_to(source / "firmware.bin")
    with pytest.raises(StatusError):
        preservation.verify(destination)


def test_archive_records_are_deterministic(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    (source / "asset").write_bytes(b"same data")
    first = preservation.create(source, tmp_path / "first", "same provenance")
    second = preservation.create(source, tmp_path / "second", "same provenance")
    assert first["manifest_sha256"] == second["manifest_sha256"]
    assert (
        first["manifest_sha256"]
        == hashlib.sha256(
            (tmp_path / "first/manifest.json").read_bytes()
        ).hexdigest()
    )
    for tree in (tmp_path / "first", tmp_path / "second"):
        for parent, _directories, filenames in os.walk(tree):
            Path(parent).chmod(0o755)
            for name in filenames:
                (Path(parent) / name).chmod(0o644)
