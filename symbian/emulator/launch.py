"""Owned foreground launches for the digest-pinned GUI emulator experiment."""

import argparse
import hashlib
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
from contextlib import contextmanager
from dataclasses import dataclass
from pathlib import Path

from symbian import toolchain
from symbian.e32 import inspect_image
from symbian.emulator import Control
from symbian.status import Code, StatusError

_ROM = "b5c1ea63cb6359270c5b7cfb1bb453594e208a01b8aeb5b5e020f37d546f7086"
_EUSER = "3cec7e1546f8ed0cf64a73fece9fdd8fe6e4976535ddffd18b7068c19c01357b"


def _digest(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def _stop(process: subprocess.Popen | None) -> None:
    """Reaps only a subprocess owned by this launcher."""
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


@dataclass
class Session:
    """Resources of one fresh instance; its owning context reaps the child."""

    process: subprocess.Popen
    directory: Path
    endpoint: Path
    symbols: Path
    source: Path
    headers: Path
    port: int
    image: dict

    def wait_ready(self, *, debug: bool, timeout: float = 20) -> None:
        """Waits without connecting to the single-client GDB listener."""
        deadline = time.monotonic() + timeout
        control = Control(self.endpoint, timeout=1)
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise StatusError(
                    Code.FAILED_PRECONDITION,
                    f"Emulator exited {self.process.returncode}; "
                    f"see {self.directory / 'frontend.log'}",
                )
            if debug:
                log = (self.directory / "frontend.log").read_text(
                    errors="replace"
                )
                if "Failed to bind gdb socket" in log:
                    raise StatusError(Code.ALREADY_EXISTS, "GDB port busy")
                if "Waiting for gdb" in log:
                    return
            else:
                try:
                    control.status()
                    return
                except StatusError as error:
                    if error.code != Code.UNAVAILABLE:
                        raise
            time.sleep(0.05)
        raise StatusError(Code.DEADLINE_EXCEEDED, "Emulator startup timed out")

    def gdb_hook(self) -> Path:
        """Relocates symbols after remote connection, before IDE breakpoints."""
        arguments = [
            str(self.directory),
            str(self.symbols),
            str(self.source),
            str(self.headers),
            self.image["code_base"],
        ]
        hook = self.directory / "remote.gdb"
        hook.write_text(
            "set pagination off\nset confirm off\n"
            "set architecture arm\nset remotetimeout 20\n"
            "python\nimport sys\n"
            f"sys.path.insert(0, {str(self.source.parents[1])!r})\n"
            "from symbian.emulator.gdb import register\n"
            f"register(*{arguments!r})\nend\n"
            "define target hookpost-remote\n"
            "  symbian-relocate\n"
            "end\n",
            encoding="utf-8",
        )
        return hook


@contextmanager
def session(root: Path, *, debug: bool = False, port: int = 24689):
    """Builds and launches one fixture copy, retaining logs and reaping it.

    Args:
        root: Prepared repository workspace; this is a bounded GUI experiment.
        debug: Starts the guest halted at its loopback GDB listener.
        port: Reserved configuration port for the IDE's remote connection.

    Yields:
        Session with an owned child and private native control endpoint.
    """
    root = root.resolve()
    golden = root / ".symbian/instances/delight-import-01"
    executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    inputs = {
        golden / "data/roms/rm-807/SYM.ROM": _ROM,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": _EUSER,
    }
    for path, expected in inputs.items():
        if not path.is_file() or _digest(path) != expected:
            raise StatusError(
                Code.FAILED_PRECONDITION,
                f"Fixture fingerprint mismatch: {path}",
            )
    if not executable.is_file():
        raise StatusError(
            Code.NOT_FOUND, f"Build patched emulator: {executable}"
        )
    if debug:
        if not 1024 <= port <= 65535:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid GDB port")
        try:
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", port))
        except OSError as error:
            raise StatusError(Code.ALREADY_EXISTS, "GDB port busy") from error
    source = root / "examples/gui_app"
    build = root / ".symbian/gui-app"
    toolchain.build(
        source, build, "/usr/bin/clang++", "/opt/homebrew/bin/ld.lld"
    )
    image = inspect_image(build / "gui_app.exe")
    if image["uid3"] != 0xE0000811 or image["dll"]:
        raise StatusError(Code.FAILED_PRECONDITION, "Unexpected GUI executable")
    output = root / ".symbian/gui-runs"
    output.mkdir(parents=True, exist_ok=True)
    directory = Path(
        tempfile.mkdtemp(prefix="debug-" if debug else "run-", dir=output)
    )
    instance = directory / "instance"
    shutil.copytree(golden, instance)
    target = instance / "data/drives/rm-807/c/sys/bin/gui_app.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(build / "gui_app.exe", target)
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        f"enable-gdb-stub: {'true' if debug else 'false'}\n"
        f"gdb-port: {port}\nlog-svc: true\n"
    )
    manifest = {
        "schema": "symbian.gui-launch/v1",
        "debug": debug,
        "gdb_port": port if debug else None,
        "inputs": {str(p): digest for p, digest in inputs.items()},
        "e32_sha256": _digest(build / "gui_app.exe"),
        "elf_sha256": _digest(build / "gui_app.elf"),
        "emulator_sha256": _digest(executable),
    }
    process = None
    try:
        with tempfile.TemporaryDirectory(
            prefix="symbian-ide-", dir="/tmp"
        ) as private:
            endpoint = Path(private) / "control.sock"
            env = os.environ.copy()
            env.update(
                EKA2L1_DATA_ROOT=str(instance),
                EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
                EKA2L1_RESEARCH_CONTROL_SOCKET=str(endpoint),
            )
            with (directory / "frontend.log").open("w") as log:
                process = subprocess.Popen(
                    [
                        str(executable),
                        "--device",
                        "RM-807",
                        "--run",
                        "C:\\sys\\bin\\gui_app.exe",
                    ],
                    cwd=directory,
                    env=env,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                )
                manifest.update(pid=process.pid, endpoint=str(endpoint))
                (directory / "launch.json").write_text(
                    json.dumps(manifest, indent=2) + "\n"
                )
                print(
                    f"Emulator session: {directory}",
                    file=sys.stderr,
                    flush=True,
                )
                active = Session(
                    process,
                    directory,
                    endpoint,
                    build / "gui_app.elf",
                    source,
                    root / ".symbian/gui-sdk/include",
                    port,
                    image,
                )
                try:
                    active.wait_ready(debug=debug)
                    yield active
                finally:
                    _stop(process)
                    saved = endpoint.with_name(endpoint.name + ".status.json")
                    if saved.is_file():
                        shutil.copyfile(
                            saved, directory / "control.sock.status.json"
                        )
    finally:
        _stop(process)
        manifest["frontend_exit"] = process.returncode if process else None
        manifest["inputs_unchanged"] = all(
            _digest(p) == d for p, d in inputs.items()
        )
        (directory / "launch.json").write_text(
            json.dumps(manifest, indent=2) + "\n"
        )


def main(argv: list[str] | None = None) -> int:
    """Launches Run or supervises the actual GDB child used by CLion Debug."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--gdb", type=Path)
    parser.add_argument("--port", type=int, default=24689)
    args, debugger_args = parser.parse_known_args(argv)
    if args.gdb and any(
        a in ("--version", "--configuration", "--help") for a in debugger_args
    ):
        os.execv(str(args.gdb), [str(args.gdb), *debugger_args])
    if not args.gdb and debugger_args:
        parser.error("Unexpected Run arguments")

    def cancel(_signum, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, cancel)
    debugger = None
    try:
        with session(args.root, debug=bool(args.gdb), port=args.port) as active:
            if args.gdb:
                debugger = subprocess.Popen(
                    [
                        str(args.gdb),
                        "-x",
                        str(active.gdb_hook()),
                        *debugger_args,
                    ]
                )
                return debugger.wait()
            return active.process.wait()
    except KeyboardInterrupt:
        return 130
    except (StatusError, OSError) as error:
        print(str(error), file=sys.stderr)
        return 1
    finally:
        _stop(debugger)


if __name__ == "__main__":
    sys.exit(main())
