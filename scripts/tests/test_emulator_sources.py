"""Checks exact installed source recipes independently of a live catalog."""

import hashlib
import importlib
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
