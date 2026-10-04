"""Launch the desktop console independently of its invoking terminal."""

import os
import subprocess
import sys
from importlib.util import find_spec
from pathlib import Path

from symbian.console.display import terminal_screen_index
from symbian.status import Code, Status


def launch_detached(workdir: Path | None = None) -> None:
    """Leave the terminal free while the GUI owns its own process lifetime."""
    directory = (workdir or Path.cwd()).expanduser().resolve()
    if not directory.is_dir():
        raise Status(
            code=Code.INVALID_ARGUMENT,
            message=f"Console working directory does not exist: {directory}",
        ).to_exception()
    frontend = (
        "symbian.console.web_frontend.app"
        if find_spec("webview") is not None
        else "symbian.console.gui"
    )
    options: dict[str, object] = {"start_new_session": os.name != "nt"}
    environment = os.environ.copy()
    if workdir is not None:
        environment["SYMBIAN_CONSOLE_EXPLICIT_WORKDIR"] = "1"
    screen_index = terminal_screen_index()
    if screen_index is not None:
        environment["SYMBIAN_CONSOLE_SCREEN_INDEX"] = str(screen_index)
    if os.name == "nt":
        options["creationflags"] = subprocess.DETACHED_PROCESS
    subprocess.Popen(
        [sys.executable, "-m", frontend],
        cwd=directory,
        stdin=subprocess.DEVNULL,
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
        close_fds=True,
        env=environment,
        **options,
    )
