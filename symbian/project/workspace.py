"""Prepares repository-owned inputs for the root CMake source workspace."""

import argparse
import fcntl
import shutil
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

from symbian.process import run
from symbian.project.sdk import stage_headers, stage_imports
from symbian.toolchain.host_tools import llvm_tool


def _progress(root: Path, message: str) -> None:
    """Reports workspace preparation in CMake and an ignored persistent log."""
    line = f"[{datetime.now(timezone.utc).isoformat()}] {message}"
    with (root / ".symbian/workspace-inputs.log").open("a") as log:
        log.write(line + "\n")
    print(f"-- Workspace inputs: {message}", flush=True)


def _acquire_with_progress(
    lock, log_path: Path, poll_seconds: float = 1.0
) -> None:
    """Relays new log entries while waiting for another preparer's lock."""
    offset = log_path.stat().st_size if log_path.exists() else 0
    while True:
        try:
            fcntl.flock(lock, fcntl.LOCK_EX | fcntl.LOCK_NB)
            return
        except BlockingIOError:
            with log_path.open() as log:
                log.seek(offset)
                while line := log.readline():
                    if line.endswith("\n"):
                        if "waiting for shared preparation lock" not in line:
                            print(
                                "-- Workspace inputs (other CMake profile): "
                                + line.rstrip(),
                                flush=True,
                            )
                        offset = log.tell()
                    else:
                        break
            time.sleep(poll_seconds)


def _publish(staged: Path, output: Path) -> None:
    """Updates generated inputs while preserving unchanged dependencies."""
    output.mkdir(parents=True, exist_ok=True)
    for source in staged.rglob("*"):
        destination = output / source.relative_to(staged)
        if source.is_symlink():
            if (
                destination.is_symlink()
                and destination.readlink() == source.readlink()
            ):
                continue
            if (
                destination.exists()
                and destination.is_dir()
                and not destination.is_symlink()
            ):
                shutil.rmtree(destination)
            else:
                destination.unlink(missing_ok=True)
            destination.symlink_to(source.readlink())
        elif source.is_dir():
            if destination.is_symlink():
                destination.unlink()
            destination.mkdir(parents=True, exist_ok=True)
        elif (
            destination.is_symlink()
            or not destination.is_file()
            or destination.read_bytes() != source.read_bytes()
        ):
            if destination.is_symlink():
                destination.unlink()
            shutil.copy2(source, destination)
    for destination in sorted(output.rglob("*"), reverse=True):
        if destination == output / ".prepared":
            continue
        source = staged / destination.relative_to(output)
        if not source.exists() and not source.is_symlink():
            if destination.is_dir() and not destination.is_symlink():
                shutil.rmtree(destination)
            else:
                destination.unlink(missing_ok=True)


def prepare(root: Path) -> Path:
    """Serializes input preparation across concurrent IDE CMake profiles."""
    root = root.resolve()
    directory = root / ".symbian"
    directory.mkdir(parents=True, exist_ok=True)
    _progress(root, "waiting for shared preparation lock")
    with (directory / "workspace-inputs.lock").open("w") as lock:
        _acquire_with_progress(lock, directory / "workspace-inputs.log")
        _progress(root, "preparation lock acquired")
        return _prepare(root)


def _prepare(root: Path) -> Path:
    """Prepares headers/imports without building or selecting an installed SDK.

    CMake owns all SDK archive builds. This view contains only shared headers,
    patched dependency sources and generated original-platform import proxies.
    """
    root = root.resolve()
    output = root / ".symbian/workspace-inputs"
    dependencies = [root / "symbian/project/sdk.py", Path(__file__)]
    dependencies.append(root / "symbian/project/graphics.py")
    dependencies.append(root / "symbian/project/qt.py")
    dependencies.append(root / "symbian/project/qtmobility.py")
    dependencies.append(root / "symbian/project/native_surface.py")
    dependencies += list((root / "research/native-sdk").glob("*.json"))
    dependencies += list((root / "research/portable").glob("*.json"))
    dependencies.append(root / "symbian/project/portable.py")
    dependencies += list((root / "symbian/toolchain/cmake").glob("*"))
    dependencies += list((root / "research/abseil").glob("*.patch"))
    dependencies += list((root / ".symbian/gui-sdk/include").glob("*"))
    compiler = llvm_tool("clang++")
    linker = llvm_tool("ld.lld", sibling=compiler.parent)
    tools = {
        name: llvm_tool(name, sibling=compiler.parent)
        for name in (
            "clang",
            "clang++",
            "llvm-ar",
            "llvm-ranlib",
            "ld.lld",
            "clang-scan-deps",
        )
    }
    stamp = output / ".prepared"
    tools_match = all(
        (output / "bin" / name).is_symlink()
        and (output / "bin" / name).readlink() == tool
        for name, tool in tools.items()
    )
    if (
        tools_match
        and stamp.exists()
        and all(
            path.stat().st_mtime <= stamp.stat().st_mtime
            for path in dependencies
            if path.is_file()
        )
    ):
        _progress(root, "cached inputs are current")
        return output
    _progress(root, "refreshing headers and frozen import interfaces")
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as directory:
        staged = Path(directory) / "inputs"
        staged.mkdir()
        (staged / "bin").mkdir()
        for name, tool in tools.items():
            (staged / "bin" / name).symlink_to(tool)
        stage_headers(root, staged, compiler)
        _progress(root, "public headers staged")
        stage_imports(
            root,
            staged,
            compiler,
            linker,
            progress=lambda message: _progress(root, message),
        )
        (staged / "cmake").mkdir()
        for directory in (
            "symbian/toolchain/cmake",
            "symbian/project/templates",
        ):
            for module in (root / directory).glob("*"):
                if module.is_file():
                    (staged / "cmake" / module.name).symlink_to(module)
        abseil = staged / "abseil"
        run(
            [
                "git",
                "clone",
                "--quiet",
                "--no-hardlinks",
                str(root / "research/upstream/abseil-cpp"),
                str(abseil),
            ],
            cwd=root,
        )
        for name in (
            "symbian-platform.patch",
            "symbian-low-level-alloc.patch",
            "symbian-container-no-elf-tls.patch",
        ):
            run(
                ["git", "apply", str(root / "research/abseil" / name)],
                cwd=abseil,
            )
        shutil.rmtree(abseil / ".git")
        # SDK-owned public headers remain live source inputs in an IDE.
        for relative, directory in (
            ("include/symbian", "cpp/symbian/concurrency/common/symbian"),
            ("include/symbian", "cpp/symbian/concurrency/guest/symbian"),
            ("include/symbian", "cpp/symbian/api/include/symbian"),
            ("include/thread", "cpp/symbian/concurrency/common/thread"),
            ("include/thread", "cpp/symbian/concurrency/guest/thread"),
            ("include/symbian/http", "cpp/symbian/http"),
            ("include/symbian/net", "cpp/symbian/net"),
            ("include/symbian/websocket", "cpp/symbian/websocket"),
            ("include/symbian/agent", "cpp/symbian/agent"),
        ):
            for header in (root / directory).rglob("*.h"):
                destination = (
                    staged / relative / header.relative_to(root / directory)
                )
                destination.parent.mkdir(parents=True, exist_ok=True)
                destination.unlink(missing_ok=True)
                destination.symlink_to(header)
        runtime_header = staged / "include/symbian/runtime.h"
        runtime_header.unlink()
        runtime_header.symlink_to(root / "cpp/symbian/runtime/abi.h")
        _publish(staged, output)
        stamp.touch()
    _progress(root, "source workspace inputs ready")
    return output


def main() -> None:
    """Prepares the root source project's generated inputs."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", required=True, type=Path)
    args = parser.parse_args()
    prepare(args.root)
    from symbian.project.ide import configure_workspace_ide

    configure_workspace_ide(args.root)


if __name__ == "__main__":
    main()
