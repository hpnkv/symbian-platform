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
