"""Declares source inputs for independently refreshed workspace stages."""

import json
from pathlib import Path

from symbian.project.input_cache import InputCache


def stage_sources(root: Path, cache: InputCache, resource: Path) -> dict:
    """Enumerates copied inputs; live owned C++ remains under Ninja's graph."""
    upstream = root / "research/upstream"
    policy = [
        root / "symbian/project" / name
        for name in (
            "workspace.py",
            "workspace_sources.py",
            "input_cache.py",
            "sdk.py",
        )
    ]
    native = json.loads(
        (root / "research/native-sdk/inventory.json").read_text()
    )
    headers = [
        *policy,
        root / "symbian/project/graphics.py",
        root / "symbian/project/native_surface.py",
        root / "symbian/project/portable.py",
        root / "research/native-sdk",
        root / "research/portable",
        root / "scripts/check_public_header_boundary.py",
        root / ".symbian/gui-sdk/include",
        resource / "include",
        root / "third_party/symbian-network-headers/include",
        root / "third_party/symbian/EPL-1.0.html",
    ]
    for relative in (
        "llvm-project/libcxx/include",
        "mimalloc/include",
        "ossrv/genericopenlibs/openenvcore/include",
        "ossrv/genericopenlibs/openenvcore/libm/include",
        "ossrv/genericopenlibs/openenvcore/libc/inc",
        "ossrv/genericopenlibs/openenvcore/libpthread/inc",
        "graphics/opengles/openglesinterface/include",
        "graphics/egl/eglinterface/include",
    ):
        headers.append(upstream / relative)
    for relative in (
        "kernelhwsrv/halservices/hal/inc/hal.h",
        "kernelhwsrv/halservices/hal/inc/hal_data.h",
        "kernelhwsrv/kernel/eka/include/e32math.h",
        "kernelhwsrv/kernel/eka/include/e32math.inl",
        "appsupport/appfw/apparchitecture/inc/AppInfo.rh",
    ):
        headers.append(upstream / relative)
    headers.append(upstream / "mm/imagingandcamerafws/camerafw/Include")
    for record in native["headers"] + native.get("licenses", []):
        if record.get("destination"):
            headers.append(upstream / record["source"])
            if record.get("generator"):
                headers.append(upstream / record["generator"]["source"])
    for manifest in (root / "research/portable").glob("*.json"):
        record = json.loads(manifest.read_text())
        source = upstream / record["source"]
        if manifest.stem in ("sdl2", "sdl3"):
            headers += [source / "include", source / "LICENSE.txt"]
            headers += list(source.parent.glob("*.tar.gz"))
        else:
            headers += [source / name for name in record.get("files", {})]
            headers += [
                source
                / ("include" if manifest.stem == "freetype" else "")
                / name
                for name in record.get("headers", {})
            ]
            headers += [source / name for name in record.get("licenses", {})]
            headers.append(
                source / ("LICENSE" if manifest.stem == "png" else "README")
            )
    # Header contents are live symlinks; membership must also be refreshed.
    owned = root / "cpp/symbian"
    headers += cache.select(owned, ("*.h",))
    headers += [
        root / "symbian/toolchain/cmake" / name
        for name in ("exe_startup.S", "exe_startup.cc", "exe_image.ld")
    ]
    imports = [
        *policy,
        root / "symbian/sdk/proxies.py",
        root / "symbian/project/graphics.py",
        root / "symbian/project/qt.py",
        root / "symbian/project/qtmobility.py",
        root / "symbian/project/native_surface.py",
        root / "research/native-sdk",
        root / "symbian/toolchain/cmake",
    ]
    for facility in native["facilities"]:
        if not facility.get("blocked"):
            imports.append(upstream / facility["definition"]["source"])
    # These trees contain the original frozen definitions used by core imports.
    for relative in (
        "kernelhwsrv/kernel/eka/eabi",
        "kernelhwsrv/halservices/hal/eabi",
        "kernelhwsrv/kernel/eka/compsupp/eabi",
        "kernelhwsrv/userlibandfileserver/fileserver/eabi",
        "graphics/windowing/windowserver/eabi",
        "graphics/graphicsdeviceinterface/gdi/eabi",
        "graphics/opengles/openglesinterface/eabi",
        "graphics/egl/eglinterface/eabi",
        "mm/imagingandcamerafws/camerafw/eabi",
        "ossrv/genericopenlibs/openenvcore/libc/eabi",
        "ossrv/genericopenlibs/openenvcore/libm/eabi",
        "ossrv/genericopenlibs/openenvcore/libpthread/eabi",
    ):
        imports.append(upstream / relative)
    imports += [
        root / "third_party/symbian-network-headers" / name
        for name in ("esockU.def", "insockU.def")
    ]
    # syncqt reads headers and qmake metadata, not Qt implementation units.
    qt = upstream / "qt4"
    imports += cache.select(qt / "src", ("*.h", "*.pro", "*.pri", "*.def"))
    imports += [
        qt / name
        for name in (
            "bin/syncqt",
            "src/s60main/newallocator_hook.cpp",
            "LICENSE.LGPL",
            "LGPL_EXCEPTION.txt",
            "LICENSE.FDL",
        )
    ]
    mobility = json.loads(
        (root / "research/native-sdk/qt-mobility.json").read_text()
    )
    mobility_source = upstream / "qtmobility"
    imports.append(mobility_source / mobility["global_header"]["source"])
    for module in mobility["modules"]:
        imports.append(mobility_source / module["definition"])
        imports += [
            mobility_source / header["source"] for header in module["headers"]
        ]
    imports += [mobility_source / name for name in mobility["licenses"]]
    abseil = [
        root / "symbian/project/workspace.py",
        upstream / "abseil-cpp",
        root / "research/abseil",
    ]
    return {"headers": headers, "imports": imports, "abseil": abseil}
