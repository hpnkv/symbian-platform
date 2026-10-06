"""Compiles public native headers through relocated installed SDK targets."""

import argparse
import json
import os
import subprocess
from pathlib import Path


def check(prefix: Path, output: Path, architecture: str) -> None:
    """Builds independent header consumers with public dependencies."""
    prefix = prefix.resolve()
    data = json.loads(
        (prefix / "share/symbian/native/inventory.json").read_text()
    )
    openc = json.loads(
        (prefix / "share/symbian/native/openc-header-usage.json").read_text()
    )
    output.mkdir(parents=True, exist_ok=True)
    project = output / architecture
    project.mkdir(exist_ok=True)
    lines = [
        "cmake_minimum_required(VERSION 3.28)",
        "project(native_surface_canaries LANGUAGES C CXX ASM)",
        "include(SymbianApp)",
        "add_custom_target(symbian_header_canaries ALL)",
    ]
    rejected = {}
    headers = {
        h["include"].casefold(): h
        for h in data["headers"]
        if h.get("destination")
    }
    for facility in data["facilities"]:
        name = facility["target"]
        reason = facility.get("blocked") or facility.get("selection_blocked")
        if reason:
            rejected[name] = reason
            continue
        for index, header in enumerate(facility["headers"]):
            if Path(header).suffix.lower() != ".h":
                continue
            target = f"canary_{name}_{index}"
            profile = (
                openc["profiles"].get(header, {}) if name == "OpenC" else {}
            )
            if profile.get("blocked"):
                rejected[f"{name}:{header}"] = profile["blocked"]
                continue
            language = profile.get(
                "language",
                openc["default_language"] if name == "OpenC" else "C++",
            )
            source = project / f"{target}.{'c' if language == 'C' else 'cc'}"
            prerequisites = headers[header.casefold()].get(
                "include_prerequisites", []
            )
            record = headers[header.casefold()]
            includes = [
                *prerequisites,
                *profile.get("preinclude", []),
                profile.get("include_via", record.get("include_via", header)),
            ]
            guard = profile.get("indirect_guard", record.get("include_guard"))
            source.write_text(
                "".join(f"#include <{h}>\n" for h in includes)
                + (
                    f"#ifndef {guard}\n"
                    '#error "Original umbrella omitted its public header"\n'
                    "#endif\n"
                    if (
                        (
                            profile.get("include_via")
                            and profile.get("indirect_guard")
                        )
                        or (
                            record.get("include_via")
                            and record.get("include_guard")
                        )
                    )
                    else ""
                )
            )
            lines += [
                f"add_library({target} OBJECT {source.name})",
                f"target_link_libraries({target} PRIVATE Symbian::{name})",
                f"add_dependencies(symbian_header_canaries {target})",
            ]
    (project / "CMakeLists.txt").write_text("\n".join(lines) + "\n")
    (project / "blocked.json").write_text(json.dumps(rejected, indent=2) + "\n")
    environment = {**os.environ}
    environment.pop("SYMBIAN_SDK_MANIFEST", None)
    command = [
        "cmake",
        "-S",
        str(project),
        "-B",
        str(project / "build"),
        "-G",
        "Ninja",
        f"-DCMAKE_TOOLCHAIN_FILE={prefix}/cmake/symbian-arm.cmake",
        f"-DSYMBIAN_SDK_PREFIX={prefix}",
        f"-DSYMBIAN_TARGET_ARCH={architecture}",
        f"-DCMAKE_CXX_COMPILER={prefix}/bin/clang++",
        f"-DCMAKE_C_COMPILER={prefix}/bin/clang",
        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
    ]
    subprocess.run(command, check=True, env=environment)
    subprocess.run(
        ["cmake", "--build", str(project / "build"), "-j", "6", "--", "-k0"],
        check=True,
        env=environment,
    )


def main() -> None:
    """Checks a caller-selected SDK and architecture."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--sdk", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument(
        "--architecture", choices=["armv5t", "armv6"], required=True
    )
    args = parser.parse_args()
    check(args.sdk, args.output, args.architecture)


if __name__ == "__main__":
    main()
