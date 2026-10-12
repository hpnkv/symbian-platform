"""Prepares repository-owned inputs for the root CMake source workspace."""

import argparse
import fcntl
import json
import shutil
import time
from datetime import datetime, timezone
from pathlib import Path

from symbian.native import require_native
from symbian.process import run
from symbian.project.input_cache import InputCache
from symbian.project.sdk import stage_headers, stage_imports
from symbian.project.workspace_sources import stage_sources
from symbian.status import Code, StatusError
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
    output.mkdir(parents=True, exist_ok=True)
    cache = InputCache(root / ".symbian/cache/workspace")
    resource = Path(
        run([str(compiler), "-print-resource-dir"], cwd=root).strip()
    )
    sources = stage_sources(root, cache, resource)
    # Tool replacements invalidate their dependent stages, not patched Abseil.
    tool_inputs = (
        list(tools.values())
        + [Path(require_native().__file__)]
        + [Path(shutil.which(name)) for name in ("cmake", "ninja", "perl")]
    )
    tool_key = cache.snapshot(tool_inputs)
    source_keys = {
        name: cache.snapshot(paths) for name, paths in sources.items()
    }
    keys = dict(source_keys)
    keys["headers"] += tool_key
    keys["imports"] += tool_key

    def headers(staged):
        stage_headers(root, staged, compiler)
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

    def imports(staged):
        # Native validation also checks the public header closure.
        # Borrow those outputs for validation without assigning their ownership
        # to this stage or rebuilding them when only a frozen DEF changes.
        borrowed = cache.state["stages"]["headers"]["files"]
        for relative in borrowed:
            source, destination = output / relative, staged / relative
            destination.parent.mkdir(parents=True, exist_ok=True)
            shutil.copyfile(source, destination)
        stage_imports(
            root,
            staged,
            compiler,
            linker,
            progress=lambda message: _progress(root, message),
        )
        for report in (staged / "proxies").glob("*/report.json"):
            data = json.loads(report.read_text())
            for field in ("artifact", "definition"):
                path = Path(data[field])
                if path.is_relative_to(staged):
                    data[field] = str(output / path.relative_to(staged))
            report.write_text(json.dumps(data, indent=2) + "\n")
        for relative in borrowed:
            (staged / relative).unlink()

    def abseil(staged):
        dependency = staged / "abseil"
        shutil.copytree(
            root / "research/upstream/abseil-cpp",
            dependency,
            ignore=shutil.ignore_patterns(".git"),
        )
        for name in (
            "symbian-platform.patch",
            "symbian-low-level-alloc.patch",
            "symbian-container-no-elf-tls.patch",
            "symbian-log-no-weak-gcov.patch",
        ):
            run(
                ["git", "apply", str(root / "research/abseil" / name)],
                cwd=dependency,
            )

    changed = []
    for name, build in (
        ("headers", headers),
        ("imports", imports),
        ("abseil", abseil),
    ):
        if cache.stage(name, keys[name], output, build):
            changed.append(name)
            _progress(root, f"{name} refreshed")
        else:
            _progress(root, f"{name} unchanged")
    # CMake module and tool paths are live links, not copied foreign inputs.
    for name, tool in tools.items():
        destination = output / "bin" / name
        destination.parent.mkdir(exist_ok=True)
        if not destination.is_symlink() or destination.readlink() != tool:
            destination.unlink(missing_ok=True)
            destination.symlink_to(tool)
    modules = list((root / "symbian/toolchain/cmake").glob("*")) + list(
        (root / "symbian/project/templates").glob("*")
    )
    cache.trees.update(
        (root / "symbian/toolchain/cmake", root / "symbian/project/templates")
    )
    module_output = output / "cmake"
    module_output.mkdir(exist_ok=True)
    for module in modules:
        if module.is_file():
            destination = module_output / module.name
            if not destination.is_symlink() or destination.readlink() != module:
                destination.unlink(missing_ok=True)
                destination.symlink_to(module)
    names = {module.name for module in modules if module.is_file()}
    for module in module_output.iterdir():
        if module.name not in names:
            module.unlink()
    if any(
        cache.snapshot(paths) != source_keys[name]
        for name, paths in sources.items()
    ) or (cache.snapshot(tool_inputs) != tool_key):
        raise StatusError(
            Code.ABORTED, "Workspace inputs changed during preparation"
        )
    if not cache.state.get("scoped_publication"):
        # Retire obsolete files from the former monolithic staging layout.
        owned = {
            relative
            for stage in cache.state["stages"].values()
            for relative in stage["files"]
        }
        owned.update("cmake/" + name for name in names)
        owned.update("bin/" + name for name in tools)
        owned.add(".prepared")
        for path in sorted(output.rglob("*"), reverse=True):
            if path.is_dir() and not path.is_symlink():
                if not any(path.iterdir()):
                    path.rmdir()
            elif str(path.relative_to(output)) not in owned:
                path.unlink()
        cache.state["scoped_publication"] = True
    cache.cmake_dependencies(root / ".symbian/workspace-dependencies.cmake")
    cache.save()
    (output / ".prepared").touch()
    _progress(
        root,
        "source workspace inputs ready: "
        + (", ".join(changed) or "all current"),
    )
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
