"""Source input publication preserves incremental CMake dependency tracking."""

from symbian.project.workspace import _publish


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
