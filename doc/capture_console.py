"""Render public Console guide captures with an isolated, sample context.

Requires the project Python environment and headless Google Chrome on macOS.
No local firmware, phone identifiers or home-directory paths enter the images.
"""

from __future__ import annotations

import json
import subprocess
from pathlib import Path

from symbian.console.web_frontend.app import render_html
from symbian.console.web_frontend.bridge import ConsoleWebBridge

ROOT = Path(__file__).resolve().parents[1]
OUTPUT = ROOT / "doc/docs/assets/screenshots"
STAGING = ROOT / ".symbian/docs-capture"
CHROME = Path("/Applications/Google Chrome.app/Contents/MacOS/Google Chrome")


def capture(name: str, catalog: dict, page: str, app: dict | None) -> None:
    """Capture the real frontend with a nonprivate documentation fixture."""
    context = {
        "workspace": "/workspace",
        "project": "/workspace/examples/gui_app" if app else None,
        "sdk_manifest": "/workspace/sdk/sdk.json",
        "devices": [],
    }
    setup = f"""
<script>
state.tasks = {json.dumps(catalog['tasks'])};
state.context = {json.dumps(context)};
state.page = {json.dumps(page)};
state.applicationOverview = {json.dumps(app)};
renderNavigation(); renderSelection(); renderMain(); setWork('Ready');
</script>
"""
    html = render_html().replace("</body>", setup + "</body>")
    source = STAGING / f"{name}.html"
    source.write_text(html)
    subprocess.run(
        [
            str(CHROME),
            "--headless=new",
            "--disable-gpu",
            "--hide-scrollbars",
            "--force-device-scale-factor=2",
            "--window-size=1440,900",
            "--virtual-time-budget=1500",
            f"--screenshot={OUTPUT / f'{name}.png'}",
            source.as_uri(),
        ],
        check=True,
        capture_output=True,
        text=True,
    )


def main() -> None:
    """Create two reproducible guide screenshots."""
    if not CHROME.is_file():
        raise RuntimeError(f"Chrome not found: {CHROME}")
    OUTPUT.mkdir(parents=True, exist_ok=True)
    STAGING.mkdir(parents=True, exist_ok=True)
    bridge = ConsoleWebBridge(workdir=ROOT, initial_application=False)
    try:
        catalog = bridge.get_catalog()
    finally:
        bridge.shutdown()
    capture("console-actions", catalog, "applications", None)
    capture(
        "console-application",
        catalog,
        "application_detail",
        {
            "directory": "/workspace/examples/gui_app",
            "caption": "Counter",
            "name": "gui_app",
            "generated": False,
            "kind": "e32-import-experiment",
            "architecture": "armv6",
            "uid3": "0xe0000811",
            "artifact": None,
            "package_name": "Counter",
            "firmware": None,
            "icon_data_url": None,
        },
    )


if __name__ == "__main__":
    main()
