"""Serialization and interpreter isolation contracts for host tooling."""

import json
import subprocess
import sys
from pathlib import Path

from symbian.emulator.launch import Session
from symbian.toolchain.project_build import Target


def test_cmake_target_metadata_roundtrips_paths_and_input_sets(tmp_path):
    value = Target(
        artifact=tmp_path / "app.elf",
        inputs=frozenset({tmp_path / "app.cc", tmp_path / "image.ld"}),
        compile_groups=[{"language": "CXX"}],
        link_fragments=[{"fragment": "--emit-relocs"}],
    )
    assert Target.model_validate_json(value.model_dump_json()) == value


def test_session_serializes_metadata_without_process_handle(tmp_path):
    child = subprocess.Popen([sys.executable, "-c", "pass"])
    try:
        value = Session(
            process=child,
            directory=tmp_path,
            endpoint=tmp_path / "socket",
            symbols=tmp_path / "app.elf",
            source=tmp_path / "src",
            headers=tmp_path / "include",
            port=24689,
            image={"code_base": 32768},
        )
        decoded = json.loads(value.model_dump_json())
        assert "process" not in decoded
        assert decoded["symbols"] == str(tmp_path / "app.elf")
        assert decoded["image"] == {"code_base": 32768}
    finally:
        child.wait(timeout=5)


def test_gdb_bridge_imports_without_site_packages_or_native_extension():
    root = Path(__file__).resolve().parents[2]
    result = subprocess.run(
        [
            sys.executable,
            "-S",
            "-c",
            "from pathlib import Path; from symbian.gdb_bridge import quote; "
            "import sys; assert 'symbian._native' not in sys.modules; "
            "assert quote(Path('/tmp/a b')) == '\"/tmp/a b\"'",
        ],
        cwd=root,
        capture_output=True,
        text=True,
        timeout=5,
        check=False,
    )
    assert result.returncode == 0, result.stderr
