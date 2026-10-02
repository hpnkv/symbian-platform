"""CMake/Ninja experimental link and native E32 conversion orchestration."""

import hashlib
import json
import re
import shutil
import tempfile
from pathlib import Path

from symbian.analysis import inspect_elf
from symbian.e32 import (
    convert_dll,
    convert_imported_executable,
    convert_pic_executable,
    inspect_image,
)
from symbian.process import run
from symbian.status import Code, StatusError
from symbian.toolchain import project_build as cmake_build
from symbian.toolchain.architecture import (
    project_architecture,
)
from symbian.toolchain.architecture import (
    target as arm_target,
)
from symbian.toolchain.builds import _compiler


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
    architecture = project_architecture(project, options, preset)
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
    dll = options.get("kind") == "e32-dll-experiment"
    imported = options.get("kind") == "e32-import-experiment"
    definition_name = options.get("export_definition")
    if dll and (not isinstance(definition_name, str) or not definition_name):
        raise StatusError(
            Code.INVALID_ARGUMENT, "DLL export_definition required"
        )
    if not dll and definition_name is not None:
        raise StatusError(
            Code.INVALID_ARGUMENT, "Exports require a DLL project"
        )
    definition = (project / definition_name).resolve() if dll else None
    if definition is not None and (
        not definition.is_relative_to(project)
        or definition.is_relative_to(output)
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Frozen DEF must stay in project"
        )
    definition_bytes = definition.read_bytes() if definition else b""
    proxy_names = options.get("import_proxies", [])
    if (
        not isinstance(proxy_names, list)
        or (imported and not 1 <= len(proxy_names) <= 16)
        or (dll and len(proxy_names) > 16)
        or (not imported and not dll and proxy_names)
        or any(
            not isinstance(path, str) or not path or ";" in path or "\0" in path
            for path in proxy_names
        )
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid import_proxies list")
    if any(path.startswith("${sdk}/") for path in proxy_names):
        from symbian.project.configuration import ProjectConfiguration
        from symbian.project.sdk import discover_sdk

        # Standalone examples can use the active SDK without pretending to be
        # generated application projects. Generated projects keep their own
        # explicit, transferable sdk-location.json selection.
        if (project / "symbian-project.json").is_file():
            prefix = ProjectConfiguration.load(project).sdk.prefix
        elif (project / "sdk-location.json").is_file():
            location = json.loads((project / "sdk-location.json").read_text())
            prefix = (project / location["sdk"]).resolve()
        else:
            prefix = discover_sdk().parent
        proxy_names = [
            str(prefix / path[7:]) if path.startswith("${sdk}/") else path
            for path in proxy_names
        ]
    proxies = tuple((project / path).resolve() for path in proxy_names)
    if len(set(proxies)) != len(proxies) or any(
        path.is_relative_to(output) for path in proxies
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Distinct proxies must stay outside output"
        )
    proxy_bytes = {path: path.read_bytes() for path in proxies}

    def convert(data: bytes) -> bytes:
        if dll:
            return convert_dll(
                data, definition_bytes, list(proxy_bytes.values()), uid3
            )
        if imported:
            return convert_imported_executable(
                data, list(proxy_bytes.values()), uid3
            )
        return convert_pic_executable(data, uid3)

    output.mkdir(parents=True, exist_ok=True)
    primary = output / "cmake"
    name = options["name"]

    def configure(tree: Path) -> cmake_build.Target:
        return cmake_build.configure(
            project,
            tree,
            name,
            preset,
            **tools,
            import_proxies=proxies,
            architecture=architecture,
        )

    target = configure(primary)
    inputs = {path: path.read_bytes() for path in target.inputs}
    inputs.update(proxy_bytes)
    if definition is not None:
        inputs[definition] = definition_bytes
    first_log = cmake_build.build(primary, name, tools["cmake"])
    _unchanged(inputs)
    # Ninja's dependency graph adds headers discovered by the compiler.
    dependencies = cmake_build.dependencies(
        primary, target.artifact, tools["ninja"]
    )
    for path in dependencies:
        inputs.setdefault(path, path.read_bytes())
    first_elf = target.artifact.read_bytes()
    metadata = inspect_elf(target.artifact)
    expected = arm_target(architecture)
    if metadata["arm_attributes"]["cpu_arch"] != expected.cpu_attribute:
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Compiler output disagrees with {architecture}: "
            f"{metadata['arm_attributes']}; "
            "remove conflicting -march/-mcpu flags and rebuild "
            "in a fresh target tree",
        )
    first_image = convert(first_elf)
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
        second_image = convert(second_elf)
        if (first_elf, first_image) != (second_elf, second_image):
            raise StatusError(
                Code.DATA_LOSS, "Independent CMake ELF/E32 builds differ"
            )
        for suffix, data in (
            ("elf", first_elf),
            ("dll" if dll else "exe", first_image),
        ):
            staged = temporary / f"result.{suffix}"
            staged.write_bytes(data)
            staged.replace(output / f"{name}.{suffix}")
    database = output / "compile_commands.json"
    shutil.copyfile(primary / "compile_commands.json", database)
    image = output / f"{name}.{'dll' if dll else 'exe'}"
    elf = output / f"{name}.elf"
    report = {
        "schema": (
            "symbian.e32-dll-experiment/v1"
            if dll
            else (
                "symbian.e32-import-experiment/v1"
                if imported
                else "symbian.e32-pic-experiment/v2"
            )
        ),
        "artifact_kind": (
            "experimental-e32-dll" if dll else "experimental-e32-executable"
        ),
        "artifact": str(image),
        "target": arm_target(architecture).model_dump(),
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
        "import_execution_verified": False,
        "limitations": [
            (
                "Frozen function exports and eager imports only; "
                "RELRO/local GOT and bounded per-process data/BSS allowed; "
                "constructor arrays need the SDK DLL entry; "
                "no TLS or general unload/lifetime contract"
                if dll
                else (
                    "Eager function imports, bounded local GOT "
                    "and EXE data/BSS; "
                    "no TLS/exports; constructors need SDK startup"
                    if imported
                    else "Internal RX pointers, RELRO and bounded local GOT; "
                    "no SDK/imports, writable data, exports, "
                    "constructors or packaging"
                )
            ),
            "All relocations must be retained by the trusted linker",
            "Hand-written absolute addresses cannot be detected",
            (
                "DLL entry/lifetime must be verified separately from conversion"
                if dll
                else "Project startup, heap initialization and cleanup "
                "are not established by conversion or static inspection"
            ),
            "Matched Belle runtime and full target ABI are unverified",
        ],
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report
