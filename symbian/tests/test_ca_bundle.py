"""Per-project trust roots must stay explicit and bounded."""

import hashlib
import shutil
import subprocess
from pathlib import Path

import pytest

from symbian.packaging.ca_bundle import selected_bundle
from symbian.status import Code, StatusError


@pytest.mark.skipif(not shutil.which("openssl"), reason="OpenSSL CLI required")
def test_selected_ca_bundle_validates_every_certificate(tmp_path: Path):
    project = tmp_path / "project"
    project.mkdir()
    build = project / "build"
    build.mkdir()
    artifact = build / "probe.elf"
    source = project / "roots.pem"
    key = tmp_path / "private.key"
    subprocess.run(
        [
            "openssl",
            "req",
            "-x509",
            "-newkey",
            "rsa:2048",
            "-nodes",
            "-keyout",
            str(key),
            "-out",
            str(source),
            "-days",
            "1",
            "-subj",
            "/CN=Project Test CA",
        ],
        check=True,
        capture_output=True,
    )
    (build / "CMakeCache.txt").write_text(
        f"CMAKE_HOME_DIRECTORY:INTERNAL={project}\n"
        f"SYMBIAN_CA_BUNDLE:FILEPATH={source}\n"
    )
    chosen = selected_bundle(project, artifact)
    assert chosen == (source, source.read_bytes())
    assert hashlib.sha256(chosen[1]).hexdigest()
    source.write_bytes(source.read_bytes() + key.read_bytes())
    with pytest.raises(StatusError) as caught:
        selected_bundle(project, artifact)
    assert caught.value.code == Code.INVALID_ARGUMENT
    source.write_text(
        "-----BEGIN CERTIFICATE-----\nQQ==\n" "-----END CERTIFICATE-----\n"
    )
    with pytest.raises(StatusError) as caught:
        selected_bundle(project, artifact)
    assert caught.value.code == Code.INVALID_ARGUMENT


def test_unset_ca_bundle_includes_no_roots(tmp_path: Path):
    assert selected_bundle(tmp_path, tmp_path / "probe.elf") is None
