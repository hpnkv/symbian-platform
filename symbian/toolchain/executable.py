"""CMake/Ninja experimental link and native E32 conversion orchestration."""

import hashlib
import json
import re
import shutil
import tempfile
from pathlib import Path

from symbian.analysis import inspect_elf
from symbian.e32 import convert_pic_executable, inspect_image
from symbian.process import run
from symbian.status import Code, StatusError
from symbian.toolchain import _compiler
from symbian.toolchain import project_build as cmake_build


def _unchanged(inputs: dict[Path, bytes]) -> None:
    if any(path.read_bytes() != data for path, data in inputs.items()):
        raise StatusError(Code.ABORTED, "Project inputs changed during build")


def build_executable(
    project: Path, output: Path, options: dict, compiler: str, linker: str
) -> dict:
    """Builds a CMake target twice, then converts its ELF in the native core.

    Args:
        project: Directory with symbian.toml, CMakeLists and CMake presets.
        output: Artifact directory; its cmake subdirectory remains usable.
        options: Validated project identity and experimental UID from TOML.
        compiler: Clang executable name or path.
        linker: LLD executable name or path, preserving its driver symlink.

    Returns:
        Build evidence, published ELF/E32 paths and the actual CMake database.
    """
    for filename in ("CMakeLists.txt", "CMakePresets.json"):
        if not (project / filename).is_file():
            raise StatusError(
                Code.INVALID_ARGUMENT, f"CMake project requires {filename}"
            )
    if any(key in options for key in ("source", "startup", "linker_script")):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Declare sources, startup and linker script in CMakeLists.txt",
        )
    uid3 = options.get("uid3")
    if type(uid3) is not int or not 0xE0000000 <= uid3 <= 0xEFFFFFFF:
        raise StatusError(Code.INVALID_ARGUMENT, "Experimental UID3 required")
    preset = options.get("cmake_preset", "symbian-pic")
    if not isinstance(preset, str) or not re.fullmatch(
        r"[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}", preset
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid CMake preset name")
    tools = {
        "compiler": _compiler(compiler),
        "linker": _compiler(linker),
        "cmake": _compiler("cmake"),
        "ninja": _compiler("ninja"),
    }
    versions = {
        name: run([path, "--version"], cwd=project)
        for name, path in tools.items()
    }
    output.mkdir(parents=True, exist_ok=True)
    primary = output / "cmake"
    name = options["name"]

    def configure(tree: Path) -> cmake_build.Target:
        return cmake_build.configure(project, tree, name, preset, **tools)

    target = configure(primary)
    inputs = {path: path.read_bytes() for path in target.inputs}
    first_log = cmake_build.build(primary, name, tools["cmake"])
    _unchanged(inputs)
    # Ninja's dependency graph adds headers discovered by the compiler.
    dependencies = cmake_build.dependencies(
        primary, target.artifact, tools["ninja"]
    )
    for path in dependencies:
        inputs.setdefault(path, path.read_bytes())
    first_elf = target.artifact.read_bytes()
    first_image = convert_pic_executable(first_elf, uid3)
    with tempfile.TemporaryDirectory(prefix="repro-", dir=output) as temporary:
        temporary = Path(temporary)
        repeated = configure(temporary)
        if repeated.inputs != target.inputs:
            raise StatusError(Code.ABORTED, "CMake input graph changed")
        second_log = cmake_build.build(temporary, name, tools["cmake"])
        repeated_dependencies = cmake_build.dependencies(
            temporary, repeated.artifact, tools["ninja"]
        )
        _unchanged(inputs)
        if repeated_dependencies != dependencies:
            raise StatusError(Code.ABORTED, "Compiler dependency graph changed")
        second_elf = repeated.artifact.read_bytes()
        second_image = convert_pic_executable(second_elf, uid3)
        if (first_elf, first_image) != (second_elf, second_image):
            raise StatusError(
                Code.DATA_LOSS, "Independent CMake ELF/E32 builds differ"
            )
        for suffix, data in (("elf", first_elf), ("exe", first_image)):
            staged = temporary / f"result.{suffix}"
            staged.write_bytes(data)
            staged.replace(output / f"{name}.{suffix}")
    database = output / "compile_commands.json"
    shutil.copyfile(primary / "compile_commands.json", database)
    image = output / f"{name}.exe"
    elf = output / f"{name}.elf"
    report = {
        "schema": "symbian.e32-pic-experiment/v2",
        "artifact_kind": "experimental-e32-executable",
        "artifact": str(image),
        "sha256": hashlib.sha256(first_image).hexdigest(),
        "linked_elf": str(elf),
        "linked_elf_sha256": hashlib.sha256(first_elf).hexdigest(),
        "inputs": {
            str(path): hashlib.sha256(data).hexdigest()
            for path, data in sorted(inputs.items())
        },
        **{
            name: {"path": path, "version": versions[name]}
            for name, path in tools.items()
        },
        "build_system": {
            "generator": "Ninja",
            "preset": preset,
            "tree": str(primary),
            "target": name,
            "compile_groups": target.compile_groups,
            "link_fragments": target.link_fragments,
            "primary_log": first_log,
            "repeated_log": second_log,
        },
        "elf": inspect_elf(elf),
        "e32": inspect_image(image),
        "reproducible": True,
        "reproducibility_scope": "two CMake trees on this host/toolchain",
        "compile_commands": str(database),
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "limitations": [
            "No SDK/imports, writable data, exports, constructors or packaging",
            "All relocations must be retained by the trusted linker",
            "Hand-written absolute addresses cannot be detected",
            "Direct thread exit skips User::Exit cleanup; no resources allowed",
            "Matched Belle runtime and full target ABI are unverified",
        ],
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report
