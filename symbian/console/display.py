"""Locate the monitor containing the terminal that launched the console."""

import os
import subprocess
import sys


def _ancestor_process_ids() -> list[int]:
    """Return the invoking process chain, starting with this process."""
    process_ids = [os.getpid()]
    current = process_ids[0]
    for _ in range(16):
        result = subprocess.run(
            ["ps", "-o", "ppid=", "-p", str(current)],
            capture_output=True,
            text=True,
            check=False,
        )
        try:
            parent = int(result.stdout.strip())
        except ValueError:
            break
        if parent <= 1 or parent in process_ids:
            break
        process_ids.append(parent)
        current = parent
    return process_ids


def terminal_screen_index() -> int | None:
    """Find the macOS screen that contains the invoking terminal window.

    Other platforms let the window manager choose the normal placement.
    """
    if sys.platform != "darwin":
        return None
    try:
        import AppKit
        import Quartz

        process_ids = _ancestor_process_ids()
        windows = Quartz.CGWindowListCopyWindowInfo(
            Quartz.kCGWindowListOptionAll, Quartz.kCGNullWindowID
        )
        screens = AppKit.NSScreen.screens()
        for process_id in process_ids:
            for window in windows:
                if window.get("kCGWindowOwnerPID") != process_id:
                    continue
                if window.get("kCGWindowLayer") != 0:
                    continue
                bounds = window.get("kCGWindowBounds", {})
                width = bounds.get("Width", 0)
                height = bounds.get("Height", 0)
                if width < 200 or height < 150:
                    continue
                centre_x = bounds["X"] + width / 2
                centre_y = bounds["Y"] + height / 2
                for index, screen in enumerate(screens):
                    display_id = int(
                        screen.deviceDescription()["NSScreenNumber"]
                    )
                    display_bounds = Quartz.CGDisplayBounds(display_id)
                    origin = display_bounds.origin
                    size = display_bounds.size
                    if (
                        origin.x <= centre_x < origin.x + size.width
                        and origin.y <= centre_y < origin.y + size.height
                    ):
                        return index
    except (ImportError, OSError, KeyError, TypeError, ValueError):
        return None
    return None
