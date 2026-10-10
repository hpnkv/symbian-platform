"""Reject original Symbian headers reachable from SDK-owned public headers."""

from __future__ import annotations

import argparse
import re
from pathlib import Path

INCLUDE = re.compile(r'^\s*#\s*include\s*[<"]([^">]+)[">]', re.MULTILINE)
OWNED_ROOTS = (
    "symbian",
    "thread",
    "host/thread",
    "portable",
    "symbian_mbedtls",
    "mbedtls",
    "psa",
)
NATIVE_ROOTS = ("native", "platform")


def violations(sdk_include: Path) -> list[str]:
    """Return include chains from owned headers to original platform headers.

    Args:
        sdk_include: The installed SDK's ``include`` directory.
    """
    sdk_include = sdk_include.resolve()
    owned = [sdk_include / name for name in OWNED_ROOTS]
    original = [sdk_include / name for name in NATIVE_ROOTS]
    findings: set[str] = set()
    visited: set[Path] = set()

    def resolve(source: Path, name: str) -> Path | None:
        for candidate in (source.parent / name, sdk_include / name):
            if candidate.is_file():
                return candidate.resolve()
        for directory in original:
            candidate = directory / name
            if candidate.is_file():
                return candidate.resolve()
        return None

    def inside(path: Path, directory: Path) -> bool:
        return path == directory or directory in path.parents

    def visit(path: Path, chain: tuple[Path, ...]) -> None:
        if path in visited:
            return
        visited.add(path)
        for name in INCLUDE.findall(path.read_text(encoding="utf-8")):
            target = resolve(path, name)
            if target is None:
                continue
            if any(inside(target, directory) for directory in original):
                display = [*chain, path, target]
                findings.add(
                    " -> ".join(
                        str(item.relative_to(sdk_include)) for item in display
                    )
                )
            elif any(inside(target, directory) for directory in owned):
                visit(target, (*chain, path))

    for directory in owned:
        if directory.is_dir():
            for header in sorted(directory.rglob("*.h")):
                visit(header.resolve(), ())
    return sorted(findings)


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk_include", type=Path)
    args = parser.parse_args()
    if not args.sdk_include.is_dir():
        parser.error("SDK include directory does not exist")
    failures = violations(args.sdk_include)
    for failure in failures:
        print(failure)
    print(f"Public header boundary: {len(failures)} violations")
    return 1 if failures else 0


if __name__ == "__main__":
    raise SystemExit(main())
