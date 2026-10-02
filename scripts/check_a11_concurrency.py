"""Verifies the exact A11 source inventory before backend adaptation."""

import argparse
import hashlib
import json
import re
import subprocess
from pathlib import Path


def verify(component: Path, upstream: Path | None = None) -> dict:
    """Checks digests, source identities and the complete local include graph.

    Args:
        component: Directory containing sources.json and the upstream snapshot.
        upstream: Optional original checkout for comparison at the pin.

    Returns:
        Source acceptance metadata, without claiming a built runtime backend.

    Raises:
        ValueError: A source digest, path, inventory or include edge differs.
    """
    manifest = json.loads((component / "sources.json").read_text())
    snapshot = component / "upstream"
    entries = manifest["files"]
    actual = {
        str(path.relative_to(snapshot))
        for path in snapshot.rglob("*")
        if path.is_file()
    }
    if actual != set(entries):
        raise ValueError("A11 snapshot inventory differs from its source pin")
    for name, entry in entries.items():
        relative = Path(name)
        if relative.is_absolute() or ".." in relative.parts:
            raise ValueError(f"Invalid source identity: {name}")
        path = snapshot / name
        if path.is_symlink():
            raise ValueError(f"Source snapshot contains a symlink: {name}")
        data = path.read_bytes()
        if hashlib.sha256(data).hexdigest() != entry["sha256"]:
            raise ValueError(f"A11 source digest differs: {name}")
        edges = set()
        quoted = re.findall(
            r'^\s*#\s*include\s*"([^"]+)"', data.decode(), re.MULTILINE
        )
        for include in quoted:
            candidates = [str(relative.parent / include)] + [
                str(Path(root) / include) for root in manifest["include_roots"]
            ]
            resolved = next(
                (item for item in candidates if item in entries), None
            )
            if resolved is None:
                raise ValueError(
                    f"Missing A11 local dependency: {name}: {include}"
                )
            edges.add(resolved)
        if edges != set(entry["includes"]):
            raise ValueError(f"A11 include graph differs: {name}")
        external = set(
            re.findall(
                r"^\s*#\s*include\s*<([^>]+)>", data.decode(), re.MULTILINE
            )
        )
        if external != set(entry["external_includes"]):
            raise ValueError(f"A11 external dependencies differ: {name}")
        if upstream:
            original = subprocess.check_output(
                [
                    "git",
                    "-C",
                    str(upstream),
                    "show",
                    f"{manifest['revision']}:{name}",
                ]
            )
            if original != data:
                raise ValueError(
                    f"Original committed A11 source differs: {name}"
                )
    return {
        "revision": manifest["revision"],
        "verified_sources": len(entries),
        "verified_local_includes": sum(
            len(entry["includes"]) for entry in entries.values()
        ),
        "built_guest_backend": False,
    }


def main() -> int:
    """Verifies the staged snapshot, optionally against the original Git pin."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "--component",
        type=Path,
        default=Path(__file__).resolve().parents[1] / "cpp/symbian/concurrency",
    )
    parser.add_argument("--upstream", type=Path)
    arguments = parser.parse_args()
    print(json.dumps(verify(arguments.component, arguments.upstream), indent=2))
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
