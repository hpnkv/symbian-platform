"""CMake/Ninja link and native E32 application conversion orchestration."""

import hashlib
import json
import re
import shlex
import shutil
import tempfile
from pathlib import Path

from symbian.analysis import inspect_elf
from symbian.e32 import (
    convert_dll,
    convert_eka1_executable,
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


_PROCESS_CAPABILITY_BITS = {
    "SwEvent": 12,
    "NetworkServices": 13,
    "ReadUserData": 15,
    "WriteUserData": 16,
}


def _process_capabilities(names: object) -> int:
    """Encode the bounded process capabilities supported by app builds."""
    if (
        not isinstance(names, list)
        or any(
            not isinstance(name, str) or name not in _PROCESS_CAPABILITY_BITS
            for name in names
        )
        or len(names) != len(set(names))
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Unsupported process capability"
        )
    return sum(1 << _PROCESS_CAPABILITY_BITS[name] for name in names)


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
            "Declare sources through the SDK CMake target helpers",
        )
    uid3 = options.get("uid3")
    if type(uid3) is not int or not 0xE0000000 <= uid3 <= 0xEFFFFFFF:
        raise StatusError(Code.INVALID_ARGUMENT, "Experimental UID3 required")
    capabilities = _process_capabilities(options.get("capabilities", []))
    preset = options.get("cmake_preset", "symbian-pic")
    if not isinstance(preset, str) or not re.fullmatch(
        r"[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}", preset
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid CMake preset name")
    architecture = project_architecture(project, options, preset)
    compiler_path = _compiler(compiler)
    compiler_bin = Path(compiler_path).parent
    tools = {
        "compiler": compiler_path,
        "linker": _compiler(linker),
        "cmake": _compiler("cmake", sibling=compiler_bin),
        "ninja": _compiler("ninja", sibling=compiler_bin),
    }
    versions = {
        name: run([path, "--version"], cwd=project)
        for name, path in tools.items()
    }
    kind = options.get("kind")
    eka1 = kind in ("e32-eka1", "e32-eka1-import")
    if eka1 and (architecture != "armv5t" or capabilities):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "EKA1 requires armv5t and has no platform-security capabilities",
        )
    dll = kind in ("e32-dll", "e32-dll-experiment")
    imported = kind in (
        "e32-import",
        "e32-import-experiment",
        "e32-eka1-import",
    )
    canonical = kind in (
        "e32-eka1",
        "e32-eka1-import",
        "e32-pic",
        "e32-import",
        "e32-dll",
    )
    definition_name = options.get("export_definition")
    if definition_name is not None and (
        not isinstance(definition_name, str) or not definition_name
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Invalid DLL export_definition"
        )
    if not dll and definition_name is not None:
        raise StatusError(
            Code.INVALID_ARGUMENT, "Exports require a DLL project"
        )
    definition = (
        (project / definition_name).resolve() if definition_name else None
    )
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
        or (imported and len(proxy_names) > 256)
        or (dll and len(proxy_names) > 256)
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
        if eka1:
            return convert_eka1_executable(
                data, uid3, list(proxy_bytes.values())
            )
        if dll:
            return convert_dll(
                data,
                definition_bytes,
                list(proxy_bytes.values()),
                uid3,
                capabilities,
            )
        if imported:
            return convert_imported_executable(
                data, list(proxy_bytes.values()), uid3, capabilities
            )
        return convert_pic_executable(data, uid3, capabilities)

    output.mkdir(parents=True, exist_ok=True)
    primary = output / "cmake"
    name = options["name"]

    declared_proxies = proxies

    def configure(tree: Path) -> cmake_build.Target:
        return cmake_build.configure(
            project,
            tree,
            name,
            preset,
            **tools,
            import_proxies=declared_proxies,
            architecture=architecture,
            cmake_variables=options.get("_cmake_variables"),
        )

    target = configure(primary)

    # CMake's evaluated link fragments include transitive imported libraries.
    # Pass the same actual ordinal libraries to the native image converter.
    def linked_proxy_paths(built_target, tree):
        linked = set(declared_proxies)
        for fragment in built_target.link_fragments:
            if fragment.get("role") == "libraries":
                for item in shlex.split(fragment["fragment"]):
                    if item.endswith(".dso"):
                        linked.add((tree / item).resolve())
        return tuple(sorted(linked))

    proxies = linked_proxy_paths(target, primary)
    proxy_bytes = {
        path: path.read_bytes()
        for path in proxies
        if not path.is_relative_to(primary)
    }
    inputs = {path: path.read_bytes() for path in target.inputs}
    inputs.update(proxy_bytes)
    if definition is not None:
        inputs[definition] = definition_bytes
    first_log = cmake_build.build(primary, name, tools["cmake"])
    _unchanged(inputs)
    # Normal library targets create their ordinal transport during this build.
    # Read those outputs after the dependency graph has finished publishing.
    proxy_bytes = {path: path.read_bytes() for path in proxies}
    if dll and definition is None:
        definition_bytes = (primary / f"{name}.def").read_bytes()
    # Ninja's dependency graph adds headers discovered by the compiler.
    dependencies = cmake_build.dependencies(
        primary, target.artifact, tools["ninja"]
    )
    for path in dependencies:
        inputs.setdefault(path, path.read_bytes())
    first_elf = target.artifact.read_bytes()
    from symbian.project.libraries import built_libraries, stage_libraries

    first_libraries = built_libraries(primary, name)
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
        repeated_proxy_bytes = {
            path: path.read_bytes()
            for path in linked_proxy_paths(repeated, temporary)
        }
        if sorted(repeated_proxy_bytes.values()) != sorted(
            proxy_bytes.values()
        ):
            raise StatusError(
                Code.DATA_LOSS, "Independent import proxies differ"
            )
        proxy_bytes = repeated_proxy_bytes
        if dll and definition is None:
            repeated_definition = (temporary / f"{name}.def").read_bytes()
            if repeated_definition != definition_bytes:
                raise StatusError(
                    Code.DATA_LOSS, "Independent DLL exports differ"
                )
        second_elf = repeated.artifact.read_bytes()
        second_image = convert(second_elf)
        if built_libraries(temporary, name) != first_libraries:
            raise StatusError(
                Code.DATA_LOSS, "Independent application DLL builds differ"
            )
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
    stage_libraries(output, name, first_libraries)
    database = output / "compile_commands.json"
    shutil.copyfile(primary / "compile_commands.json", database)
    image = output / f"{name}.{'dll' if dll else 'exe'}"
    elf = output / f"{name}.elf"
    if canonical:
        schema = (
            "symbian.e32-dll/v1"
            if dll
            else "symbian.e32-import/v1" if imported else "symbian.e32-pic/v1"
        )
        artifact_kind = "e32-dll" if dll else "e32-executable"
    else:
        schema = (
            "symbian.e32-dll-experiment/v1"
            if dll
            else (
                "symbian.e32-import-experiment/v1"
                if imported
                else "symbian.e32-pic-experiment/v2"
            )
        )
        artifact_kind = (
            "experimental-e32-dll" if dll else "experimental-e32-executable"
        )
    if eka1:
        schema = (
            "symbian.e32-eka1-import/v1" if imported else "symbian.e32-eka1/v1"
        )
    report = {
        "kernel": "eka1" if eka1 else "eka2",
        "schema": schema,
        "artifact_kind": artifact_kind,
        "artifact": str(image),
        "target": {
            **arm_target(architecture).model_dump(),
            **({"e32_cpu": 0x2000} if eka1 else {}),
        },
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
                "EKA1 no-UI process, optional EUSER PE imports; no data/BSS, "
                "pointer fixups, runtime, unwinding or packaging; "
                "execution needs the tested emulator bootstrap"
                if eka1
                else (
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
                        else "Internal RX pointers, RELRO/local GOT; "
                        "no SDK/imports, writable data, exports, "
                        "constructors or packaging"
                    )
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
            (
                "EKA1 C++/imports and physical-device behavior are unverified"
                if eka1
                else "Matched Belle runtime and full target ABI are unverified"
            ),
        ],
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report
