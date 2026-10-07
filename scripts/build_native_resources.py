"""Compiles the resource tools once per native SDK host architecture."""

import argparse
import os
from pathlib import Path

from symbian.project.sdk import _install_resource_tools


def main() -> None:
    """Builds the actual resource compiler and UID checksum executable."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--host-include", type=Path, required=True)
    args = parser.parse_args()
    host_include = args.host_include.resolve()
    if not (host_include / "absl/base/nullability.h").is_file():
        raise SystemExit(
            f"Host Abseil headers are required for uidcrc: {host_include}"
        )
    (args.output / "bin").mkdir(parents=True)
    (args.output / "licenses").mkdir()
    previous = os.environ.get("CPATH")
    os.environ["CPATH"] = os.pathsep.join(
        part for part in (str(host_include), previous) if part
    )
    try:
        _install_resource_tools(args.output / "bin", args.workspace)
    finally:
        if previous is None:
            del os.environ["CPATH"]
        else:
            os.environ["CPATH"] = previous


if __name__ == "__main__":
    main()
