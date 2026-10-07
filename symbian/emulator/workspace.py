"""Builds source SDK examples for the repository's saved IDE launchers."""

import shutil
from pathlib import Path

from symbian.process import run
from symbian.status import Code, StatusError


def build_example(root: Path, name: str, output: Path, firmware: Path) -> None:
    """Publishes the host project's source-built ARMv6 example and symbols.

    Args:
        root: Repository containing the host debug CMake preset.
        name: Supported application target.
        output: Owned directory for the paired ELF and E32 outputs.
        firmware: Selected Z drive for graphics availability checks.
    """
    if name not in ("gl_app", "qt_app_classic", "sdl2_app"):
        raise StatusError(Code.INVALID_ARGUMENT, "Unknown workspace example")
    cmake = shutil.which("cmake")
    if not cmake:
        raise StatusError(Code.NOT_FOUND, "CMake is required for source builds")
    run(
        [
            cmake,
            "--preset",
            "debug",
            f"-DSYMBIAN_GRAPHICS_FIRMWARE_DIR={firmware}",
            "-DSYMBIAN_TARGET_ARCH=armv6",
            "-DSYMBIAN_BUILD_GUI_EXAMPLE=ON",
        ],
        cwd=root,
        timeout=120,
    )
    run(
        [cmake, "--build", "--preset", "debug", "--target", f"{name}_e32"],
        cwd=root,
        timeout=600,
    )
    guest = root / "build/debug/guest-armv6"
    output.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(guest / f"{name}.elf", output / f"{name}.elf")
    shutil.copyfile(
        guest / f"examples/{name}/e32/{name}.exe", output / f"{name}.exe"
    )
