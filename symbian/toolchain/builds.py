"""ARM application object builds with explicit compatibility limits."""

import hashlib
import json
import re
import shutil
import tempfile
import tomllib
from pathlib import Path

from symbian.analysis import inspect_elf
from symbian.process import run
from symbian.status import Code, StatusError

FLAGS = (
    "--target=armv6-none-eabi",
    "-mthumb",
    "-mfloat-abi=soft",
    "-mabi=aapcs",
    "-ffreestanding",
    "-fno-exceptions",
    "-fno-rtti",
    "-nostdinc",
    "-O2",
    "-std=c++20",
)

PROBE_SOURCE = (
    'extern "C" unsigned int SymbianAbiProbe(unsigned int value) {\n'
    "  return (value * 17U) ^ 0x808U;\n"
    "}\n"
)


def _compiler(name: str) -> str:
    path = shutil.which(name)
    if path is None:
        raise StatusError(Code.NOT_FOUND, f"Tool not found: {name}")
    # Multi-call LLVM tools select their driver using argv[0]. Preserve the
    # final symlink name (ld.lld may resolve to the generic lld executable).
    return str(Path(path).absolute())


def _build_object(
    source: Path,
    output: Path,
    compiler: str,
    name: str,
    architecture: str = "armv6",
) -> dict:
    from symbian.toolchain.architecture import target

    selected = target(architecture)
    flags = (f"--target={selected.triple}", *FLAGS[1:])
    compiler = _compiler(compiler)
    version = run([compiler, "--version"], cwd=source.parent)
    source_bytes = source.read_bytes()
    output.mkdir(parents=True, exist_ok=True)
    compiled = []
    with tempfile.TemporaryDirectory(prefix="probe-", dir=output) as temporary:
        temporary = Path(temporary)
        for index in range(2):
            tree = temporary / str(index)
            tree.mkdir()
            run(
                [compiler, *flags, "-c", str(source), "-o", "probe.o"], cwd=tree
            )
            obj = tree / "probe.o"
            metadata = inspect_elf(obj)
            if (
                metadata["machine"] != 40
                or metadata["type"] != 1
                or metadata["flags"] >> 24 != 5
                or metadata["flags"] & 0x400
                or metadata["arm_attributes"]["cpu_arch"]
                != selected.cpu_attribute
            ):
                raise StatusError(
                    Code.FAILED_PRECONDITION,
                    "Compiler did not produce ARM EABI5 soft-float objects",
                )
            compiled.append(obj.read_bytes())
        if compiled[0] != compiled[1]:
            raise StatusError(
                Code.DATA_LOSS, "Two isolated object builds differ"
            )
        if source.read_bytes() != source_bytes:
            raise StatusError(Code.ABORTED, "Source changed during build")
        object_path = output / f"{name}.o"
        (temporary / "result.o").write_bytes(compiled[0])
        (temporary / "result.o").replace(object_path)

    commands = [
        {
            "directory": str(source.parent),
            "file": str(source),
            "arguments": [
                compiler,
                *flags,
                "-c",
                str(source),
                "-o",
                str(object_path),
            ],
            "output": str(object_path),
        }
    ]
    database = output / "compile_commands.json"
    database.write_text(json.dumps(commands, indent=2) + "\n", encoding="utf-8")
    report = {
        "schema": "symbian.arm-object/v1",
        "target": selected.model_dump(),
        "artifact_kind": "arm-elf-relocatable",
        "artifact": str(object_path),
        "sha256": hashlib.sha256(compiled[0]).hexdigest(),
        "source_sha256": hashlib.sha256(source_bytes).hexdigest(),
        "compiler": {"path": compiler, "version": version},
        "flags": list(flags),
        "elf": metadata,
        "reproducible": True,
        "reproducibility_scope": "two builds on this host/toolchain",
        "compile_commands": str(database),
        "symbian_loader_verified": False,
        "limitations": [
            "No SDK, link, E32 conversion, packaging or runtime validation",
            "No proof of target C++ layout, imports or leave compatibility",
        ],
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report


def probe(
    output: Path, compiler: str = "clang++", *, architecture: str = "armv6"
) -> dict:
    """Proves SDK-free ARM object generation and local byte reproducibility."""
    output = output.resolve()
    output.mkdir(parents=True, exist_ok=True)
    source = output / "probe.cc"
    source.write_text(PROBE_SOURCE, encoding="utf-8")
    return _build_object(source, output, compiler, "abi_probe", architecture)


def build(
    project: Path,
    output: Path,
    compiler: str = "clang++",
    linker: str = "ld.lld",
    *,
    architecture: str | None = None,
) -> dict:
    """Builds a declared object or E32 application and its clangd database."""
    project = project.resolve()
    try:
        manifest = tomllib.loads(
            (project / "symbian.toml").read_text(encoding="utf-8")
        )
    except (tomllib.TOMLDecodeError, UnicodeDecodeError) as error:
        raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
    options = manifest.get("project")
    if not isinstance(options, dict):
        raise StatusError(Code.INVALID_ARGUMENT, "Missing [project] table")
    if options.get("kind") not in (
        "arm-object",
        "e32-pic",
        "e32-import",
        "e32-dll",
        "arm-object-experiment",
        "e32-pic-experiment",
        "e32-import-experiment",
        "e32-dll-experiment",
    ):
        raise StatusError(
            Code.UNIMPLEMENTED,
            "Only ARM object and E32 application/DLL builds are supported",
        )
    if architecture is not None:
        from symbian.toolchain.architecture import target

        options = {**options, "architecture": target(architecture).architecture}
    name = options.get("name")
    filename = options.get("source")
    if not isinstance(name, str) or not re.fullmatch(
        r"[a-zA-Z0-9][a-zA-Z0-9_-]{0,63}", name
    ):
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid project name")
    if options["kind"] in (
        "e32-pic",
        "e32-import",
        "e32-dll",
        "e32-pic-experiment",
        "e32-import-experiment",
        "e32-dll-experiment",
    ):
        from symbian.toolchain.executable import build_executable

        return build_executable(
            project, output.resolve(), options, compiler, linker
        )
    if not isinstance(filename, str):
        raise StatusError(Code.INVALID_ARGUMENT, "Missing source filename")
    if "\0" in filename:
        raise StatusError(Code.INVALID_ARGUMENT, "Invalid source filename")
    source = (project / filename).resolve()
    if not source.is_relative_to(project) or not source.is_file():
        raise StatusError(
            Code.INVALID_ARGUMENT, "Source must be a file within the project"
        )
    return _build_object(
        source,
        output.resolve(),
        compiler,
        name,
        options.get("architecture", "armv6"),
    )
