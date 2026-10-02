"""Actual A11 snapshot identity and dependency closure acceptance."""

import importlib.util
import json
import shutil
from pathlib import Path

import pytest

ROOT = Path(__file__).parents[2]
COMPONENT = ROOT / "cpp/symbian/concurrency"


def checker():
    """Loads the standalone, standard-library-only provenance checker."""
    spec = importlib.util.spec_from_file_location(
        "a11_source_checker", ROOT / "scripts/check_a11_concurrency.py"
    )
    module = importlib.util.module_from_spec(spec)
    spec.loader.exec_module(module)
    return module.verify


def test_actual_snapshot_has_closed_local_dependencies():
    result = checker()(COMPONENT)
    assert result["revision"] == "fcccb8cb6e1e67d7ba0822ac14cece9ee4c7091b"
    assert result["verified_sources"] >= 46
    assert result["built_guest_backend"] is False


@pytest.mark.parametrize("change", ["bytes", "missing", "extra", "edge"])
def test_source_pin_detects_tampering(tmp_path, change):
    component = tmp_path / "concurrency"
    shutil.copytree(COMPONENT, component)
    source = component / "upstream/cpp/a11/concurrency/future.h"
    if change == "bytes":
        source.write_bytes(source.read_bytes() + b"\n")
    elif change == "missing":
        source.unlink()
    elif change == "extra":
        (component / "upstream/extra.h").touch()
    else:
        path = component / "sources.json"
        manifest = json.loads(path.read_text())
        manifest["files"]["cpp/a11/concurrency/future.h"]["includes"] = []
        path.write_text(json.dumps(manifest))
    with pytest.raises(ValueError):
        checker()(component)
