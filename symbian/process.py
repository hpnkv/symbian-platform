"""Bounded host process execution without shell interpretation."""

import os
import shlex
import subprocess
from collections.abc import Mapping
from pathlib import Path
from threading import Lock, Thread

from symbian.status import Code, StatusError


def run(
    argv: list[str],
    *,
    cwd: Path,
    timeout: float = 30,
    env: Mapping[str, str] | None = None,
) -> str:
    """Runs a host tool and returns UTF-8 output or a structured failure."""
    log_path = os.environ.get("SYMBIAN_CONSOLE_BUILD_LOG")
    if log_path:
        return _run_with_live_log(
            argv, cwd=cwd, timeout=timeout, env=env, log_path=Path(log_path)
        )
    try:
        result = subprocess.run(
            argv,
            cwd=cwd,
            capture_output=True,
            text=True,
            encoding="utf-8",
            errors="replace",
            timeout=timeout,
            check=False,
            env=env,
        )
    except subprocess.TimeoutExpired as error:
        raise StatusError(
            Code.DEADLINE_EXCEEDED, f"Tool timed out: {argv[0]}"
        ) from error
    except FileNotFoundError as error:
        raise StatusError(
            Code.NOT_FOUND, f"Tool not found: {argv[0]}"
        ) from error
    if result.returncode != 0:
        details = (result.stderr or result.stdout)[-4000:].strip()
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Tool exited {result.returncode}: {argv[0]}: {details}",
        )
    return result.stdout.strip()


def _run_with_live_log(
    argv: list[str],
    *,
    cwd: Path,
    timeout: float,
    env: Mapping[str, str] | None,
    log_path: Path,
) -> str:
    """Capture the usual result while copying tool output to the GUI."""
    stdout: list[str] = []
    stderr: list[str] = []
    write_lock = Lock()
    try:
        with log_path.open("a", encoding="utf-8") as log:
            log.write(f"$ {shlex.join(argv)}\n")
            log.flush()
            process = subprocess.Popen(
                argv,
                cwd=cwd,
                stdout=subprocess.PIPE,
                stderr=subprocess.PIPE,
                text=True,
                encoding="utf-8",
                errors="replace",
                env=env,
            )

            def copy_lines(pipe: object, destination: list[str]) -> None:
                assert pipe is not None
                for line in pipe:
                    destination.append(line)
                    with write_lock:
                        log.write(line)
                        log.flush()

            readers = [
                Thread(target=copy_lines, args=(process.stdout, stdout)),
                Thread(target=copy_lines, args=(process.stderr, stderr)),
            ]
            for reader in readers:
                reader.start()
            try:
                returncode = process.wait(timeout=timeout)
            except subprocess.TimeoutExpired as error:
                process.kill()
                process.wait()
                for reader in readers:
                    reader.join()
                raise StatusError(
                    Code.DEADLINE_EXCEEDED, f"Tool timed out: {argv[0]}"
                ) from error
            for reader in readers:
                reader.join()
    except FileNotFoundError as error:
        raise StatusError(
            Code.NOT_FOUND, f"Tool not found: {argv[0]}"
        ) from error
    if returncode != 0:
        details = ("".join(stderr) or "".join(stdout))[-4000:].strip()
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Tool exited {returncode}: {argv[0]}: {details}",
        )
    return "".join(stdout).strip()
