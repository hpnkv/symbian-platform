"""Isolate CLI workflows that may replace their process with an SDK tool."""

import os
import subprocess
import sys
from pathlib import Path
from typing import Any, Literal

from pydantic import BaseModel, Field

from symbian.console.models import CommandRequest, CommandResult
from symbian.status import Status, StatusCode


class CliResponse(BaseModel):
    """Canonical machine response from one SDK CLI child process."""

    schema_name: Literal["symbian.cli/v1"] = Field(
        alias="schema", description="CLI result schema"
    )
    status: Status = Field(description="SDK operation status")
    result: Any | None = Field(
        default=None,
        description="Command-specific result payload",
        exclude_if=lambda value: value is None,
    )


def run_cli(request: CommandRequest) -> CommandResult:
    """Run a catalogued CLI task without replacing the desktop process."""
    command = [
        sys.executable,
        "-m",
        "symbian.cli",
        *request.path,
        *request.argv,
        "--output-format=json",
    ]
    if request.path == ("init",) and "--non-interactive" not in command:
        command.append("--non-interactive")
    try:
        environment = os.environ.copy()
        if request.sdk_manifest:
            environment["SYMBIAN_SDK_MANIFEST"] = request.sdk_manifest
        if request.build_log_path:
            environment["SYMBIAN_CONSOLE_BUILD_LOG"] = request.build_log_path
        if request.run_session_path:
            environment["SYMBIAN_CONSOLE_RUN_SESSION_PATH"] = (
                request.run_session_path
            )
        if request.path in {("app", "run"), ("emu", "run")}:
            environment["SYMBIAN_CONSOLE_FOREGROUND_EMULATOR"] = "1"
        options = dict(
            stdin=subprocess.DEVNULL,
            text=True,
            encoding="utf-8",
            errors="replace",
            cwd=request.cwd,
            env=environment,
            check=False,
        )
        if request.path == ("app", "run") and request.build_log_path:
            # The CLI's stdout is its one JSON response. Keep stderr visible
            # while the foreground emulator is still running.
            with open(request.build_log_path, "a", encoding="utf-8") as log:
                completed = subprocess.run(
                    command, stdout=subprocess.PIPE, stderr=log, **options
                )
        else:
            completed = subprocess.run(command, capture_output=True, **options)
    except OSError as error:
        raise Status(
            code=StatusCode.UNAVAILABLE,
            message=f"Could not start SDK command: {error}",
        ).to_exception() from error
    try:
        response = CliResponse.model_validate_json(completed.stdout)
    except ValueError as error:
        if request.path == ("app", "run") and request.build_log_path:
            detail = (
                Path(request.build_log_path)
                .read_text(encoding="utf-8", errors="replace")
                .strip()[-1000:]
            )
        else:
            detail = (completed.stderr or completed.stdout).strip()[-1000:]
        raise Status(
            code=StatusCode.DATA_LOSS,
            message=(
                "SDK command did not return a valid result"
                + (f": {detail}" if detail else "")
            ),
        ).to_exception() from error
    response.status.raise_if_not_ok()
    if completed.returncode != 0:
        raise Status(
            code=StatusCode.INTERNAL,
            message=f"SDK command exited {completed.returncode} after success",
        ).to_exception()
    return CommandResult(path=request.path, result=response.result)
