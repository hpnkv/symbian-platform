"""Content snapshots and scoped publication for generated workspace inputs."""

import fnmatch
import hashlib
import json
import os
import shutil
import tempfile
from pathlib import Path


def file_digest(path: Path) -> str:
    """Hashes an input without relying on its modification time."""
    return hashlib.sha256(path.read_bytes()).hexdigest()


class InputCache:
    """Tracks independent preparation stages under the workspace lock.

    Metadata only accelerates hashing: size, inode, mtime AND ctime must match.
    Content keys determine reuse, including additions, deletions and symlinks.
    """

    def __init__(self, directory: Path):
        self.directory = directory
        self.directory.mkdir(parents=True, exist_ok=True)
        self.state_path = directory / "state.json"
        try:
            self.state = json.loads(self.state_path.read_text())
        except (OSError, ValueError):
            self.state = {}
        self.digests = {}
        self.dependencies = set()
        self.trees = set()
        self.globs = set()

    def select(self, tree: Path, patterns: tuple[str, ...]) -> list[Path]:
        """Selects relevant tree inputs and watches their future membership."""
        self.globs.update(str(tree / pattern) for pattern in patterns)
        paths = []
        for parent, directories, names in os.walk(tree):
            directories[:] = sorted(d for d in directories if d != ".git")
            paths.extend(
                Path(parent) / name
                for name in sorted(names)
                if any(fnmatch.fnmatchcase(name, p) for p in patterns)
            )
        return paths

    def snapshot(self, paths: list[Path]) -> str:
        """Fingerprints files and trees, excluding Git data."""
        records = {}
        for path in paths:
            self.dependencies.add(path)
            if path.is_dir():
                self.trees.add(path)
                for parent, directories, names in os.walk(path):
                    directories[:] = sorted(
                        d for d in directories if d != ".git"
                    )
                    for name in sorted(names):
                        item = Path(parent) / name
                        records[str(item)] = self._digest(item)
            else:
                records[str(path)] = self._digest(path)
        return hashlib.sha256(
            json.dumps(records, sort_keys=True).encode()
        ).hexdigest()

    def _digest(self, path: Path):
        self.dependencies.add(path)
        try:
            info = path.stat()
            identity = [
                info.st_dev,
                info.st_ino,
                info.st_size,
                info.st_mtime_ns,
                info.st_ctime_ns,
            ]
            link = str(path.readlink()) if path.is_symlink() else None
        except FileNotFoundError:
            return None
        old = self.digests.get(
            str(path), self.state.get("digests", {}).get(str(path), {})
        )
        if old.get("identity") == identity and old.get("link") == link:
            digest = old["sha256"]
        else:
            digest = file_digest(path)
        self.digests[str(path)] = {
            "identity": identity,
            "link": link,
            "sha256": digest,
        }
        return [digest, link]

    def stage(self, name: str, key: str, output: Path, build) -> bool:
        """Publishes one changed stage, retaining other outputs."""
        previous = self.state.get("stages", {}).get(name, {})
        files = previous.get("files", {})
        if previous.get("key") == key and all(
            self._digest(output / relative) == digest
            for relative, digest in files.items()
        ):
            return False
        with tempfile.TemporaryDirectory(dir=self.directory) as temporary:
            staged = Path(temporary) / "stage"
            staged.mkdir()
            build(staged)
            current = {}
            for source in staged.rglob("*"):
                if not source.is_file() and not source.is_symlink():
                    continue
                relative = str(source.relative_to(staged))
                destination = output / relative
                destination.parent.mkdir(parents=True, exist_ok=True)
                if source.is_symlink():
                    if (
                        not destination.is_symlink()
                        or destination.readlink() != source.readlink()
                    ):
                        destination.unlink(missing_ok=True)
                        destination.symlink_to(source.readlink())
                elif (
                    destination.is_symlink()
                    or not destination.is_file()
                    or file_digest(source) != file_digest(destination)
                ):
                    destination.unlink(missing_ok=True)
                    shutil.copy2(source, destination)
                    # A replacement checkout/archive can carry older mtimes.
                    # Changed bytes must still invalidate Ninja consumers.
                    destination.touch()
                current[relative] = self._digest(destination)
            for relative in files.keys() - current.keys():
                (output / relative).unlink(missing_ok=True)
            self.state.setdefault("stages", {})[name] = {
                "key": key,
                "files": current,
            }
        return True

    def save(self):
        """Atomically records successful preparation and its content index."""
        self.state["digests"] = self.digests
        temporary = self.state_path.with_suffix(".tmp")
        temporary.write_text(json.dumps(self.state, sort_keys=True) + "\n")
        temporary.replace(self.state_path)

    def cmake_dependencies(self, path: Path):
        """Makes CMake observe edits and tree membership before Ninja builds."""

        def quote(value):
            return (
                '"'
                + str(value)
                .replace("\\", "/")
                .replace('"', '\\"')
                .replace(";", "\\;")
                + '"'
            )

        lines = ["# Generated workspace preparation dependencies."]
        # CONFIGURE_DEPENDS observes added/deleted inputs as well as edits.
        patterns = self.globs | {str(tree / "*") for tree in self.trees}
        for index, pattern in enumerate(sorted(patterns)):
            lines.append(
                f"file(GLOB_RECURSE workspace_tree_{index} "
                "CONFIGURE_DEPENDS LIST_DIRECTORIES FALSE "
                f"{quote(pattern)})"
            )
        inputs = sorted(
            p
            for p in self.dependencies
            if p.is_file()
            and not p.is_relative_to(path.parent / "workspace-inputs")
        )
        lines.append(
            "set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS\n"
            + "\n".join(quote(p) for p in inputs)
            + ")"
        )
        contents = "\n".join(lines) + "\n"
        if not path.is_file() or path.read_text() != contents:
            path.write_text(contents)
