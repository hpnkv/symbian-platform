"""Checks the delivered archive after relocation with clean Qt preferences."""

import argparse
import json
import os
import platform
import shutil
import subprocess
import tarfile
import tempfile
from pathlib import Path


def main():
    """Checks real executable/query, Qt platform startup and deployed assets."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("archive", type=Path)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(
        prefix="emulator archive acceptance "
    ) as d:
        root = Path(d)
        original = root / "unpacked bundle"
        original.mkdir()
        with tarfile.open(args.archive) as archive:
            archive.extractall(original, filter="data")
        relocated = root / "moved bundle"
        original.rename(relocated)
        declaration = json.loads((relocated / "emulator.json").read_text())
        frontend = relocated / declaration["frontend"]
        importer = relocated / declaration["importer"]
        env = {
            key: value
            for key, value in os.environ.items()
            if not key.startswith(("QT_", "QML", "DYLD_", "LD_", "EKA2L1_"))
        }
        env["PATH"] = "/usr/bin:/bin"
        env["HOME"] = str(root / "clean home")
        env["EKA2L1_DATA_ROOT"] = str(root / "private instance")

        def run(executable, *arguments, accepted=(0,)):
            result = subprocess.run(
                [str(executable), *arguments],
                cwd=root,
                env=env,
                text=True,
                stderr=subprocess.STDOUT,
                stdout=subprocess.PIPE,
                timeout=30,
            )
            assert result.returncode in accepted, result.stdout
            return result.stdout

        actual = json.loads(run(frontend, "--symbian-sdk-capabilities"))
        assert actual["version"] == declaration["version"]
        assert actual["control_protocol"] == declaration["control_protocol"]
        assert set(actual["capabilities"]) == set(declaration["capabilities"])
        assert "--listdevices" in run(frontend, "--help")
        assert "Usage" in run(importer, "--help", accepted=(2,))
        instance = Path(env["EKA2L1_DATA_ROOT"])
        for directory in ("resources", "patch", "scripts", "compat"):
            assert (instance / directory).is_dir(), directory
        assert list((relocated / "licenses/Qt").rglob("LICENSE*"))
        if platform.system() == "Darwin":
            subprocess.run(
                [
                    "codesign",
                    "--verify",
                    "--deep",
                    "--strict",
                    str(relocated / "EKA2L1.app"),
                ],
                check=True,
            )
        # A second clean instance must not reuse the first instance's state.
        marker = instance / "private-marker"
        marker.write_text("first instance\n")
        env["EKA2L1_DATA_ROOT"] = str(root / "second instance")
        assert "--listdevices" in run(frontend, "--help")
        assert not (Path(env["EKA2L1_DATA_ROOT"]) / marker.name).exists()
        assert marker.read_text() == "first instance\n"
        # A fresh install has no kernel until firmware is configured. An
        # explicit control request must reject that state without crashing or
        # blocking in a setup dialog.
        with tempfile.TemporaryDirectory(
            prefix="emulator-control-", dir="/tmp"
        ) as private:
            endpoint = Path(private) / "control.sock"
            env["EKA2L1_RESEARCH_CONTROL_SOCKET"] = str(endpoint)
            diagnostic = run(frontend, accepted=(2,))
            assert "configure firmware first" in diagnostic, diagnostic
            assert not endpoint.exists()
        shutil.rmtree(instance)
    print(
        "Relocated emulator: query, Qt startup, importer, assets, "
        "isolated roots and missing-firmware control rejection passed"
    )


if __name__ == "__main__":
    main()
