"""Compiles the resource tools once per native SDK host architecture."""

import argparse
from pathlib import Path

from symbian.project.sdk import _install_resource_tools


def main() -> None:
    """Builds the actual resource compiler and UID checksum executable."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    (args.output / "bin").mkdir(parents=True)
    (args.output / "licenses").mkdir()
    _install_resource_tools(args.output / "bin", args.workspace)


if __name__ == "__main__":
    main()
