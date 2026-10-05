"""Checks exact installed source recipes independently of a live catalog."""

import hashlib
import importlib
import io
import json
from pathlib import Path

import pytest


@pytest.fixture
def sources(monkeypatch):
    monkeypatch.syspath_prepend(str(Path(__file__).parents[1]))
    return importlib.import_module("emulator_sources")


def test_installed_recipe_mirrors_and_resources_have_verified_sources(
    tmp_path,
    monkeypatch,
    sources,
):
    prefix = tmp_path / "bundle"
    (prefix / "licenses/tools/library").mkdir(parents=True)
    keg = tmp_path / "installed-keg"
    (keg / ".brew").mkdir(parents=True)
    checksum = hashlib.sha256(b"actual source").hexdigest()
    recipe = (
        'url "https://example.invalid/old-version.tar.gz"\n'
        'mirror "https://example.invalid/mirror.tar.gz"\n'
        f'sha256 "{checksum}"\n'
        'resource "patch" do\n'
        '  url "https://example.invalid/fix.patch"\n'
        f'  sha256 "{checksum}"\nend\n'
    )
    (keg / ".brew/library.rb").write_text(recipe)
    monkeypatch.setattr(
        sources.os, "uname", lambda: type("Host", (), {"sysname": "Darwin"})()
    )
    monkeypatch.setattr(
        sources.subprocess, "check_output", lambda argv, **kwargs: str(keg)
    )
    downloads = []

    def download(argv, **kwargs):
        downloads.append(argv[4])
        Path(argv[-1]).write_bytes(b"actual source")

    monkeypatch.setattr(sources, "run", download)
    destination = tmp_path / "source"
    sources.runtime_sources(prefix, destination)
    assert downloads == [
        "https://example.invalid/old-version.tar.gz",
        "https://example.invalid/fix.patch",
    ]
    assert (destination / "library/library.rb").read_text() == recipe


def test_installed_source_checksum_mismatch_fails(
    tmp_path, monkeypatch, sources
):
    prefix = tmp_path / "bundle"
    (prefix / "licenses/tools/library").mkdir(parents=True)
    keg = tmp_path / "installed-keg"
    (keg / ".brew").mkdir(parents=True)
    (keg / ".brew/library.rb").write_text(
        'url "https://example.invalid/source.tar.gz"\n' f'sha256 "{"0" * 64}"\n'
    )
    monkeypatch.setattr(
        sources.os, "uname", lambda: type("Host", (), {"sysname": "Darwin"})()
    )
    monkeypatch.setattr(
        sources.subprocess, "check_output", lambda argv, **kwargs: str(keg)
    )
    monkeypatch.setattr(
        sources,
        "run",
        lambda argv, **kwargs: Path(argv[-1]).write_bytes(b"different source"),
    )
    with pytest.raises(RuntimeError, match="checksum differs"):
        sources.runtime_sources(prefix, tmp_path / "source")


@pytest.mark.parametrize("altered", [False, True])
def test_archived_ubuntu_exact_version_and_descriptor_checksums(
    tmp_path,
    monkeypatch,
    sources,
    altered,
):
    body = b"the archived source"
    checksum = hashlib.sha256(body).hexdigest()
    descriptor = (
        "Format: 3.0 (quilt)\nChecksums-Sha256:\n"
        f" {checksum} {len(body)} library_1.0.orig.tar.gz\n\n"
    ).encode()
    prefix = "https://source.test/library/1.0-1/"

    def lookup(url, **kwargs):
        if "getPublishedSources" in url:
            assert "version=1.0-1" in url
            value = {
                "entries": [
                    {
                        "source_package_name": "library",
                        "source_package_version": "1.0-1",
                        "self_link": "https://api.test/sourcepub/1",
                    }
                ]
            }
        else:
            value = [
                prefix + "library_1.0-1.dsc",
                prefix + "library_1.0.orig.tar.gz",
            ]
        return io.BytesIO(json.dumps(value).encode())

    def download(argv, **kwargs):
        path = Path(argv[-1])
        path.write_bytes(
            descriptor
            if path.suffix == ".dsc"
            else (b"x" * len(body) if altered else body)
        )

    monkeypatch.setattr(sources.urllib.request, "urlopen", lookup)
    monkeypatch.setattr(sources, "run", download)
    if altered:
        with pytest.raises(
            RuntimeError, match="Ubuntu source checksum differs"
        ):
            sources.ubuntu_source("library=1.0-1", tmp_path)
    else:
        sources.ubuntu_source("library=1.0-1", tmp_path)
        assert (tmp_path / "library_1.0.orig.tar.gz").read_bytes() == body
