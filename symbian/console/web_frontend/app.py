"""Native desktop shell for the serverless console frontend."""

import os
from base64 import b64encode
from importlib.resources import files
from pathlib import Path

import webview

from symbian.console.web_frontend.bridge import ConsoleWebBridge


def render_html() -> str:
    """Bundle local assets into one page without opening a network port."""
    resources = files("symbian.console.web_frontend")
    html = resources.joinpath("index.html").read_text(encoding="utf-8")
    css = resources.joinpath("style.css").read_text(encoding="utf-8")
    results = resources.joinpath("result_views.js").read_text(encoding="utf-8")
    live = resources.joinpath("live_views.js").read_text(encoding="utf-8")
    nokia_808 = b64encode(
        resources.joinpath("assets/nokia-808-pureview.png").read_bytes()
    ).decode("ascii")
    live = live.replace(
        "__NOKIA_808_IMAGE__", f"data:image/png;base64,{nokia_808}"
    )
    javascript = resources.joinpath("app.js").read_text(encoding="utf-8")
    return (
        html.replace("/* __CONSOLE_STYLE__ */", css)
        .replace("/* __CONSOLE_RESULTS__ */", results)
        .replace("/* __CONSOLE_LIVE__ */", live)
        .replace("/* __CONSOLE_SCRIPT__ */", javascript)
    )


def main() -> int:
    """Open a WebKit/WebView2 desktop window backed by the SDK service."""
    bridge = ConsoleWebBridge(
        workdir=Path.cwd(),
        initial_application=(
            os.environ.get("SYMBIAN_CONSOLE_EXPLICIT_WORKDIR") == "1"
        ),
    )
    try:
        screen_index = int(os.environ["SYMBIAN_CONSOLE_SCREEN_INDEX"])
        screen = webview.screens[screen_index]
    except (KeyError, ValueError, IndexError):
        screen = None
    window = webview.create_window(
        "Symbian Console",
        html=render_html(),
        js_api=bridge,
        width=1260,
        height=850,
        min_size=(860, 600),
        background_color="#f6f7f9",
        text_select=True,
        screen=screen,
    )
    bridge.attach_window(window)
    window.events.closed += bridge.shutdown
    webview.start()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
