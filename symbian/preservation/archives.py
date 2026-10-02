"""Sealed, verifiable copies of already preserved host-side artifacts."""

import hashlib
import json
import os
import stat
from pathlib import Path, PurePosixPath

from symbian.status import Code, StatusError

SCHEMA = "symbian.preservation/v1"
CHUNK_SIZE = 1024 * 1024


def _files(root: Path) -> list[Path]:
    if root.is_symlink() or not root.is_dir():
        raise StatusError(Code.INVALID_ARGUMENT, "Expected a real directory")
    files = []
    for parent, directories, filenames in os.walk(root, followlinks=False):
        for name in directories + filenames:
            path = Path(parent) / name
            mode = path.lstat().st_mode
            if not (stat.S_ISDIR(mode) or stat.S_ISREG(mode)):
                raise StatusError(
                    Code.INVALID_ARGUMENT,
                    f"Archive inputs must be regular files/directories: {path}",
                )
        files.extend(Path(parent) / name for name in filenames)
    return sorted(files, key=lambda path: path.relative_to(root).as_posix())


def _digest(path: Path) -> tuple[int, str]:
    descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
    with os.fdopen(descriptor, "rb") as stream:
        before = os.fstat(stream.fileno())
        if not stat.S_ISREG(before.st_mode):
            raise StatusError(Code.INVALID_ARGUMENT, "Expected a regular file")
        digest = hashlib.sha256()
        size = 0
        while chunk := stream.read(CHUNK_SIZE):
            size += len(chunk)
            digest.update(chunk)
        after = os.fstat(stream.fileno())
    if (before.st_size, before.st_mtime_ns, before.st_ctime_ns) != (
        after.st_size,
        after.st_mtime_ns,
        after.st_ctime_ns,
    ) or size != before.st_size:
        raise StatusError(Code.ABORTED, f"File changed while reading: {path}")
    return size, digest.hexdigest()


def create(source: Path, archive: Path, provenance: str = "unknown") -> dict:
    """Copies a quiescent regular-file tree into a new, read-only archive.

    The owner's permissions remain reversible. Keep a separately trusted
    manifest digest and a second offline copy for preservation.
    """
    if source.is_symlink() or archive.is_symlink():
        raise StatusError(
            Code.INVALID_ARGUMENT, "Archive roots cannot be links"
        )
    source = source.resolve()
    archive = archive.resolve()
    if archive == source or archive.is_relative_to(source):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Archive must be outside source"
        )
    files = _files(source)
    if not files:
        raise StatusError(Code.INVALID_ARGUMENT, "Source has no artifacts")
    archive.parent.mkdir(parents=True, exist_ok=True)
    try:
        archive.mkdir()
    except FileExistsError as error:
        raise StatusError(
            Code.ALREADY_EXISTS, "Archive already exists"
        ) from error
    incomplete = archive / ".incomplete"
    incomplete.write_text("Archive creation is incomplete\n", encoding="utf-8")
    records = []
    for path in files:
        before_size, before_hash = _digest(path)
        destination = archive / "artifacts" / path.relative_to(source)
        destination.parent.mkdir(parents=True, exist_ok=True)
        descriptor = os.open(path, os.O_RDONLY | os.O_NOFOLLOW | os.O_NONBLOCK)
        with os.fdopen(descriptor, "rb") as input_stream:
            if not stat.S_ISREG(os.fstat(input_stream.fileno()).st_mode):
                raise StatusError(
                    Code.INVALID_ARGUMENT, "Input changed file type"
                )
            with destination.open("xb") as output_stream:
                while chunk := input_stream.read(CHUNK_SIZE):
                    output_stream.write(chunk)
                output_stream.flush()
                os.fsync(output_stream.fileno())
        size, digest = _digest(destination)
        if (size, digest) != (before_size, before_hash) or _digest(path) != (
            size,
            digest,
        ):
            raise StatusError(
                Code.ABORTED, f"Source changed during copy: {path}"
            )
        records.append(
            {
                "path": destination.relative_to(archive).as_posix(),
                "size": size,
                "sha256": digest,
            }
        )
    if _files(source) != files:
        raise StatusError(Code.ABORTED, "Source file set changed during copy")
    manifest = {"schema": SCHEMA, "provenance": provenance, "files": records}
    manifest_bytes = (
        json.dumps(manifest, sort_keys=True, indent=2, ensure_ascii=True) + "\n"
    ).encode("utf-8")
    manifest_path = archive / "manifest.json"
    with manifest_path.open("xb") as stream:
        stream.write(manifest_bytes)
        stream.flush()
        os.fsync(stream.fileno())
    incomplete.unlink()
    for parent, _directories, filenames in os.walk(archive, topdown=False):
        for filename in filenames:
            (Path(parent) / filename).chmod(0o444)
        Path(parent).chmod(0o555)
    return verify(archive, hashlib.sha256(manifest_bytes).hexdigest())


def verify(archive: Path, manifest_sha256: str | None = None) -> dict:
    """Checks artifacts and optionally a separately trusted manifest digest."""
    files = _files(archive)
    if (archive / ".incomplete").exists():
        raise StatusError(Code.FAILED_PRECONDITION, "Archive is incomplete")
    manifest_path = archive / "manifest.json"
    raw = manifest_path.read_bytes()
    actual_digest = hashlib.sha256(raw).hexdigest()
    if manifest_sha256 is not None and manifest_sha256 != actual_digest:
        raise StatusError(
            Code.DATA_LOSS, "Manifest does not match trusted digest"
        )
    try:
        manifest = json.loads(raw)
    except (ValueError, UnicodeDecodeError) as error:
        raise StatusError(
            Code.DATA_LOSS, "Invalid archive manifest JSON"
        ) from error
    if not isinstance(manifest, dict) or manifest.get("schema") != SCHEMA:
        raise StatusError(Code.DATA_LOSS, "Unknown archive manifest schema")
    records = manifest.get("files")
    if not isinstance(records, list) or not records:
        raise StatusError(Code.DATA_LOSS, "Manifest has no file records")
    expected = {"manifest.json"}
    actual = {path.relative_to(archive).as_posix() for path in files}
    for record in records:
        if not isinstance(record, dict):
            raise StatusError(Code.DATA_LOSS, "Invalid manifest record")
        name = record.get("path")
        if not isinstance(name, str):
            raise StatusError(Code.DATA_LOSS, "Invalid manifest path")
        relative = PurePosixPath(name)
        if (
            relative.is_absolute()
            or len(relative.parts) < 2
            or relative.parts[0] != "artifacts"
            or ".." in relative.parts
            or "\0" in name
            or relative.as_posix() != name
            or name in expected
        ):
            raise StatusError(
                Code.DATA_LOSS, "Unsafe or duplicate manifest path"
            )
        if (
            type(record.get("size")) is not int
            or record["size"] < 0
            or not isinstance(record.get("sha256"), str)
        ):
            raise StatusError(Code.DATA_LOSS, "Invalid manifest size or digest")
        expected.add(name)
        if name not in actual:
            raise StatusError(Code.DATA_LOSS, f"Missing artifact: {name}")
        path = archive / relative
        if _digest(path) != (record["size"], record["sha256"]):
            raise StatusError(
                Code.DATA_LOSS, f"Artifact digest mismatch: {name}"
            )
    if actual != expected:
        raise StatusError(
            Code.DATA_LOSS, "Archive file set differs from manifest"
        )
    return {
        "schema": "symbian.archive-verification/v1",
        "archive": str(archive.resolve()),
        "manifest_sha256": actual_digest,
        "trusted_manifest_checked": manifest_sha256 is not None,
        "file_count": len(records),
        "verified": True,
        "permissions_sealed": all(
            not path.stat().st_mode & 0o222
            for path in {archive, *files, *(path.parent for path in files)}
        ),
        "protection": "owner-reversible permissions; offline copy needed",
    }
