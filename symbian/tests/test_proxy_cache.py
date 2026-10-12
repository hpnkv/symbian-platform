"""Real reproducible import builds with incremental cache controls."""

import json
from concurrent.futures import ThreadPoolExecutor
from pathlib import Path

from symbian.sdk import build_import_proxy, inspect_proxy
from symbian.sdk.proxies import import_proxy_cache


def test_proxy_cache_scopes_def_edits_and_recovers_corruption(
    tmp_path, monkeypatch
):
    import symbian.sdk.proxies as proxies

    definition = tmp_path / "input.def"
    definition.write_text("EXPORTS\nFirst @ 7 NONAME\n")
    cache = tmp_path / "cache"
    with import_proxy_cache(cache) as counts:
        first = build_import_proxy(definition, [], "first.dll", tmp_path / "a")

        # Keep the loaded recipe after an IDE source edit while waiting.
        def changed_source(function):
            raise AssertionError(
                "Recipe source read after module initialization"
            )

        monkeypatch.setattr(proxies.inspect, "getsource", changed_source)
        second = build_import_proxy(definition, [], "first.dll", tmp_path / "b")
        assert counts == {"built": 1, "reused": 1}
        assert first["sha256"] == second["sha256"]
        assert first["cache_entry"] == second["cache_entry"]
        assert Path(second["compile_commands"]).is_file()
        assert all(Path(path).is_file() for path in second["inputs"])
        definition.write_text("EXPORTS\nSecond @ 9 NONAME\n")
        changed = build_import_proxy(
            definition, [], "first.dll", tmp_path / "c"
        )
        assert counts == {"built": 2, "reused": 1}
        assert changed["sha256"] != first["sha256"]
        canonical = Path(changed["cache_entry"])
        report = json.loads((canonical / "report.json").read_text())
        Path(report["artifact"]).write_bytes(b"corrupt")
        repaired = build_import_proxy(
            definition, [], "first.dll", tmp_path / "d"
        )
        assert counts == {"built": 3, "reused": 1}
        assert inspect_proxy(Path(repaired["artifact"])) == changed["proxy"]


def test_concurrent_preparers_share_one_verified_build(tmp_path):
    definition = tmp_path / "input.def"
    definition.write_text("EXPORTS\nShared @ 17 NONAME\n")

    def prepare(index):
        with import_proxy_cache(tmp_path / "cache") as counts:
            result = build_import_proxy(
                definition, [], "shared.dll", tmp_path / str(index)
            )
        return counts, result

    with ThreadPoolExecutor(2) as workers:
        results = list(workers.map(prepare, range(2)))
    assert sum(c["built"] for c, _ in results) == 1
    assert sum(c["reused"] for c, _ in results) == 1
    assert results[0][1]["sha256"] == results[1][1]["sha256"]


def test_proxy_toolchain_edit_invalidates_verified_entry(tmp_path, monkeypatch):
    import shutil

    import symbian.sdk.proxies as proxies

    package = tmp_path / "toolchain"
    shutil.copytree(Path(str(proxies.files("symbian.toolchain"))), package)
    monkeypatch.setattr(proxies, "files", lambda name: package)
    definition = tmp_path / "input.def"
    definition.write_text("EXPORTS\nFunction @ 7 NONAME\n")
    with import_proxy_cache(tmp_path / "cache") as counts:
        before = build_import_proxy(definition, [], "test.dll", tmp_path / "a")
        toolchain = package / "cmake/symbian-arm.cmake"
        toolchain.write_text(toolchain.read_text() + "\n# changed toolchain\n")
        after = build_import_proxy(definition, [], "test.dll", tmp_path / "b")
        assert counts == {"built": 2, "reused": 0}
        assert before["cache_entry"] != after["cache_entry"]
        assert before["sha256"] == after["sha256"]
