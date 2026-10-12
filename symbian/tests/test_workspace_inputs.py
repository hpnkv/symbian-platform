"""Source input publication preserves incremental CMake dependency tracking."""

import fcntl
import threading
import time

from symbian.project.workspace import (
    _acquire_with_progress,
    _progress,
    _publish,
)


def test_preparation_progress_is_visible_and_persistent(tmp_path, capsys):
    (tmp_path / ".symbian").mkdir()
    _progress(tmp_path, "building Qt Mobility imports")
    assert "building Qt Mobility imports" in capsys.readouterr().out
    assert (
        "building Qt Mobility imports"
        in (tmp_path / ".symbian/workspace-inputs.log").read_text()
    )


def test_waiting_cmake_profile_sees_active_preparation_log(tmp_path, capsys):
    directory = tmp_path / ".symbian"
    directory.mkdir()
    log = directory / "workspace-inputs.log"
    log.write_text("[earlier] stale entry\n")
    lock_path = directory / "workspace-inputs.lock"
    with lock_path.open("w") as holder, lock_path.open("w") as waiter:
        fcntl.flock(holder, fcntl.LOCK_EX)

        def finish_active_preparation():
            time.sleep(0.03)
            with log.open("a") as output:
                output.write("[current] base-platform imports 40/131\n")
            time.sleep(0.06)
            fcntl.flock(holder, fcntl.LOCK_UN)

        worker = threading.Thread(target=finish_active_preparation)
        worker.start()
        _acquire_with_progress(waiter, log, poll_seconds=0.01)
        worker.join()
        output = capsys.readouterr().out
        assert "base-platform imports 40/131" in output
        assert "stale entry" not in output


def test_publish_preserves_unchanged_files_and_live_source_links(tmp_path):
    source = tmp_path / "source"
    source.mkdir()
    header = source / "api.h"
    header.write_text("old")
    staged = tmp_path / "staged"
    staged.mkdir()
    (staged / "header.h").symlink_to(header)
    (staged / "config.h").write_text("configured")
    output = tmp_path / "inputs"
    _publish(staged, output)
    original_mtime = (output / "config.h").stat().st_mtime_ns
    _publish(staged, output)
    assert (output / "config.h").stat().st_mtime_ns == original_mtime
    header.write_text("new")
    assert (output / "header.h").read_text() == "new"
    (staged / "config.h").write_text("changed configuration")
    _publish(staged, output)
    assert (output / "config.h").read_text() == "changed configuration"
    (staged / "header.h").unlink()
    _publish(staged, output)
    assert not (output / "header.h").exists()
    assert header.read_text() == "new"


def test_changing_input_links_never_writes_into_source_files(tmp_path):
    original = tmp_path / "original"
    original.mkdir()
    header = original / "header.h"
    header.write_text("source header")
    staged, output = tmp_path / "staged", tmp_path / "output"
    staged.mkdir()
    output.mkdir()
    (output / "header.h").symlink_to(header)
    (output / "nested").symlink_to(original, target_is_directory=True)
    (staged / "header.h").write_text("generated header")
    (staged / "nested").mkdir()
    (staged / "nested/config.h").write_text("generated config")
    _publish(staged, output)
    assert header.read_text() == "source header"
    assert not (original / "config.h").exists()
    assert not (output / "header.h").is_symlink()
    assert not (output / "nested").is_symlink()
    assert (output / "header.h").read_text() == "generated header"


def test_stage_reuse_is_scoped_and_preserves_dependency_mtimes(tmp_path):
    from symbian.project.input_cache import InputCache

    sources = tmp_path / "sources"
    sources.mkdir()
    definition = sources / "test.def"
    patch = sources / "abseil.patch"
    definition.write_text("Function @ 1")
    patch.write_text("initial patch")
    output = tmp_path / "output"
    calls = []

    def prepare():
        cache = InputCache(tmp_path / "cache")
        for name, source in (("proxy", definition), ("abseil", patch)):

            def build(staged, name=name, source=source):
                calls.append(name)
                (staged / name).write_text(source.read_text())

            cache.stage(name, cache.snapshot([source]), output, build)
        cache.save()

    prepare()
    stamp = (output / "proxy").stat().st_mtime_ns
    prepare()
    assert calls == ["proxy", "abseil"]
    patch.write_text("changed patch")
    prepare()
    assert calls == ["proxy", "abseil", "abseil"]
    assert (output / "proxy").stat().st_mtime_ns == stamp
    definition.write_text("Function @ 2")
    prepare()
    assert calls[-1] == "proxy"
    assert (output / "proxy").read_text() == "Function @ 2"
    (output / "proxy").write_text("corrupted")
    prepare()
    assert calls[-1] == "proxy"
    assert (output / "proxy").read_text() == "Function @ 2"


def test_snapshots_notice_preserved_mtime_additions_and_removals(tmp_path):
    import os

    from symbian.project.input_cache import InputCache

    source = tmp_path / "source"
    source.mkdir()
    header = source / "header.h"
    header.write_text("old")
    cache = InputCache(tmp_path / "cache")
    original = cache.snapshot([source])
    cache.save()
    timestamp = header.stat().st_mtime_ns
    header.write_text("new")
    os.utime(header, ns=(timestamp, timestamp))
    cache = InputCache(tmp_path / "cache")
    changed = cache.snapshot([source])
    assert changed != original
    added = source / "added.h"
    added.write_text("new header")
    assert cache.snapshot([source]) != changed
    added.unlink()
    assert cache.snapshot([source]) == changed
    header.unlink()
    assert cache.snapshot([source]) not in (original, changed)


def test_failed_stage_keeps_previous_outputs_and_retry_is_not_cached(tmp_path):
    import pytest

    from symbian.project.input_cache import InputCache

    cache = InputCache(tmp_path / "cache")
    output = tmp_path / "output"
    cache.stage(
        "headers",
        "before",
        output,
        lambda stage: (stage / "old.h").write_text("old"),
    )
    cache.save()

    def fail(stage):
        (stage / "new.h").write_text("partial")
        raise ValueError("Input validation failed")

    with pytest.raises(ValueError, match="validation"):
        cache.stage("headers", "after", output, fail)
    assert (output / "old.h").read_text() == "old"
    assert not (output / "new.h").exists()
    cache = InputCache(tmp_path / "cache")
    assert cache.stage(
        "headers",
        "after",
        output,
        lambda stage: (stage / "new.h").write_text("complete"),
    )
    assert not (output / "old.h").exists()
    assert (output / "new.h").read_text() == "complete"


def test_live_cpp_edits_leave_foreign_header_inputs_current(tmp_path):
    from symbian.project.input_cache import InputCache

    source = tmp_path / "owned"
    source.mkdir()
    (source / "public.h").write_text("API")
    implementation = source / "implementation.cc"
    implementation.write_text("before")
    cache = InputCache(tmp_path / "cache")
    before = cache.snapshot(cache.select(source, ("*.h",)))
    cache.save()
    implementation.write_text("after")
    (source / "new.cc").write_text("new implementation")
    cache = InputCache(tmp_path / "cache")
    assert cache.snapshot(cache.select(source, ("*.h",))) == before
    (source / "new.h").write_text("new API")
    assert cache.snapshot(cache.select(source, ("*.h",))) != before
    (source / "public.h").unlink()
    assert cache.snapshot(cache.select(source, ("*.h",))) != before
    cmake = tmp_path / "dependencies.cmake"
    cache.cmake_dependencies(cmake)
    assert "CONFIGURE_DEPENDS" in cmake.read_text()
    assert "*.h" in cmake.read_text()
    assert "implementation.cc" not in cmake.read_text()


def test_changed_copied_bytes_get_fresh_ninja_timestamp(tmp_path):
    import os

    from symbian.project.input_cache import InputCache

    cache = InputCache(tmp_path / "cache")
    output = tmp_path / "output"

    def build(staged):
        file = staged / "foreign.h"
        file.write_text("new bytes from an older archive")
        os.utime(file, (1, 1))

    started = time.time_ns()
    assert cache.stage("headers", "changed", output, build)
    assert (output / "foreign.h").stat().st_mtime_ns >= started
    stamp = (output / "foreign.h").stat().st_mtime_ns
    assert not cache.stage("headers", "changed", output, build)
    assert (output / "foreign.h").stat().st_mtime_ns == stamp
