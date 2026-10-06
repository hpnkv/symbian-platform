"""Stages the original Symbian Khronos headers and frozen import ABIs."""

import shutil
from pathlib import Path

from symbian.sdk import build_import_proxy


def stage_graphics_headers(workspace: Path, output: Path) -> None:
    """Preserves platform handle types, header layouts and license notices."""
    graphics = workspace / "research/upstream/graphics"
    gles = graphics / "opengles/openglesinterface/include"
    egl = graphics / "egl/eglinterface/include"
    include = output / "include/graphics"
    (include / "GLES").mkdir(parents=True)
    for name in (
        "gl.h",
        "glext.h",
        "glplatform.h",
        "glextplatform.h",
        "openglesuids.hrh",
        "egl.h",
        "egltypes.h",
    ):
        shutil.copyfile(gles / name, include / "GLES" / name)
    shutil.copytree(gles / "legacy_egl_1_1", include / "GLES/legacy_egl_1_1")
    shutil.copytree(gles / "GLES2", include / "GLES2")
    shutil.copytree(egl / "1.4", include / "EGL")
    shutil.copyfile(egl / "egluids.hrh", include / "EGL/egluids.hrh")
    shutil.copyfile(egl / "1.4/khronos_types.h", include / "khronos_types.h")
    (include / "KHR").mkdir()
    shutil.copyfile(egl / "khrplatform.h", include / "KHR/khrplatform.h")


def stage_graphics_imports(
    workspace: Path, output: Path, compiler: Path, linker: Path
) -> None:
    """Builds full ordinal proxies, without bundling device implementations."""
    graphics = workspace / "research/upstream/graphics"
    for dll, definition in (
        ("libglesv1_cm", "opengles/openglesinterface/eabi/libglesv1_cm11u.def"),
        ("libglesv2", "opengles/openglesinterface/eabi/libglesv2u.def"),
        ("libegl", "egl/eglinterface/eabi/libegl14U.def"),
    ):
        build_import_proxy(
            graphics / definition,
            [],
            dll + ".dll",
            output / "proxies" / dll,
            str(compiler),
            str(linker),
        )
