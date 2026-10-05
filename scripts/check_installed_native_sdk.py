"""Checks archive installation and app packaging with an installed wheel."""

import argparse
import json
import os
import shutil
import subprocess
import sys
import tempfile
from pathlib import Path


def main() -> None:
    """Runs the documented build/sign path with clean configuration and PATH."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    archive = args.archive.resolve()
    with tempfile.TemporaryDirectory(
        prefix="wheel native SDK acceptance "
    ) as d:
        root = Path(d)
        env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(
                ("SYMBIAN_", "CMAKE_", "LD_LIBRARY_PATH", "DYLD_LIBRARY_PATH")
            )
        }
        env["PATH"] = f"{Path(sys.executable).parent}:/usr/bin:/bin"
        env["HOME"] = str(root)
        env["XDG_CONFIG_HOME"] = str(root / "config")
        env["XDG_DATA_HOME"] = str(root / "data")

        def command(*arguments: str) -> None:
            subprocess.run(
                [sys.executable, "-m", "symbian.cli", *arguments],
                cwd=root,
                env=env,
                check=True,
                timeout=660,
            )

        sdk = root / "installed SDK"
        project = root / "hello_time"
        command("sdk", "install", str(sdk), "--archive", str(archive))
        command(
            "init",
            str(project),
            "--name",
            "hello_time",
            "--non-interactive",
            "--ide",
            "none",
            "--uid3",
            "0xe0000830",
        )
        command("app", "build", "--project", str(project))
        image = project / ".symbian/build/hello_time.exe"
        package = project / ".symbian/package"
        command(
            "package",
            "--project",
            str(project),
            "--artifact",
            str(image),
            "--output",
            str(package),
        )
        command(
            "signing",
            "create",
            "--identity",
            "developer",
            "--common-name",
            "Local developer",
        )
        signed = package / "hello_time-signed.sis"
        command(
            "signing",
            "sign",
            str(package / "hello_time.sis"),
            "--identity",
            "developer",
            "--destination",
            str(signed),
        )
        command("inspect", "--format", "sis", str(signed))
        counter = root / "gui_app"
        shutil.copytree(sdk / "examples/gui_app", counter)
        (counter / "sdk-location.json").write_text(
            json.dumps({"sdk": str(sdk)})
        )
        counter_build = root / "gui_app_build"
        command(
            "build", "--project", str(counter), "--output", str(counter_build)
        )
        command(
            "package",
            "--project",
            str(counter),
            "--artifact",
            str(counter_build / "gui_app.exe"),
            "--output",
            str(root / "counter_package"),
        )
        if not signed.stat().st_size:
            raise RuntimeError("Signing did not create the application package")
    print(
        "Installed wheel: native archive install, GUI build, "
        "package and signing passed"
    )


if __name__ == "__main__":
    main()
