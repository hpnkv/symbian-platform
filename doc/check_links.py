"""Checks local documentation links before MkDocs rewrites them."""

from __future__ import annotations

import re
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SITE_SOURCE = ROOT / "doc/docs"
LOCAL_LINK = re.compile(r"\]\(([^)]+)\)")
EXTERNAL = ("http:", "https:", "mailto:", "data:", "#")
GENERATED = {"cpp/index.html"}
LOCAL_RESEARCH = ROOT / "research/upstream"


def main() -> int:
    """Reports stale local links and nonlowercase article filenames."""
    errors: list[str] = []
    pages = [ROOT / "README.md", *SITE_SOURCE.rglob("*.md")]
    pages.extend((ROOT / ".dev").rglob("*.md"))
    for page in pages:
        if page.is_relative_to(SITE_SOURCE) and page.name != page.name.lower():
            errors.append(f"{page.relative_to(ROOT)}: article name is not lowercase")
        for target in LOCAL_LINK.findall(page.read_text()):
            if target.startswith(EXTERNAL):
                continue
            path = target.split("#", 1)[0]
            if not path:
                continue
            if page == SITE_SOURCE / "cpp.md" and path in GENERATED:
                continue
            resolved = (page.parent / path).resolve()
            if resolved.is_relative_to(LOCAL_RESEARCH):
                continue  # Private/ignored upstream checkout is optional in CI.
            if not resolved.exists():
                errors.append(f"{page.relative_to(ROOT)}: missing {target}")
            elif page.is_relative_to(SITE_SOURCE) and not resolved.is_relative_to(
                SITE_SOURCE
            ):
                errors.append(f"{page.relative_to(ROOT)}: outside site {target}")
    for error in errors:
        print(error)
    return 1 if errors else 0


if __name__ == "__main__":
    raise SystemExit(main())
