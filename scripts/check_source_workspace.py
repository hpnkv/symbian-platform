"""Checks source CMake profiles with no usable installed SDK selection."""

import argparse
import json
import os
import subprocess
import tempfile
from pathlib import Path


def main() -> None:
    """Checks reloads and source builds for both ARM architectures."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--host", action="store_true")
    args = parser.parse_args()
    root, output = args.workspace.resolve(), args.output.resolve()
    with tempfile.TemporaryDirectory(prefix="symbian-unselected-") as directory:
        environment = dict(os.environ)
        environment.update(
            SYMBIAN_HOME=directory,
            SYMBIAN_SDK_MANIFEST=str(Path(directory) / "missing/sdk.json"),
        )
        profiles = ["guest-probes-armv6", "guest-probes-armv5t"]
        if args.host:
            profiles += ["debug"]
        for profile in profiles:
            binary = output / profile
            configure = [
                "cmake",
                "--preset",
                profile,
                "-B",
                str(binary),
                "-DSYMBIAN_SDK_PREFIX=/sdk-that-must-never-be-used",
                "-DSYMBIAN_USE_ACTIVE_SDK=ON",
            ]
            for _ in range(2):
                subprocess.run(configure, cwd=root, env=environment, check=True)
            targets = (
                ["gui_app_e32", "gui_app_run"]
                if profile == "debug"
                else [
                    "symbian_probe_index",
                    "gui_app",
                    "qt_app_classic",
                    "agent_service",
                ]
            )
            build = [
                "cmake",
                "--build",
                str(binary),
                "--target",
                *targets,
                "-j",
                "8",
            ]
            subprocess.run(build, cwd=root, env=environment, check=True)
            # The second build checks automatic regeneration and incremental
            # dependency handling, including the host's guest sub-build.
            subprocess.run(build, cwd=root, env=environment, check=True)
            commands = json.loads(
                (binary / "compile_commands.json").read_text()
            )
            assert commands
            assert not any(
                "/sdk-that-must-never-be-used" in row["command"]
                for row in commands
            )
            if profile != "debug":
                for suffix in (
                    "examples/gui_app/app.cc",
                    "examples/qt_app_classic/app.cc",
                ):
                    row = next(
                        row for row in commands if row["file"].endswith(suffix)
                    )
                    assert "--target=arm" in row["command"]
                    assert (
                        str(root / "cpp/symbian/runtime/include")
                        in row["command"]
                    )
        print("Source workspace reload, build and selection checks passed")


if __name__ == "__main__":
    main()
