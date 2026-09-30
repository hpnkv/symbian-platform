"""Clang/LLD experimental link and native E32 conversion orchestration."""

import hashlib
import json
import tempfile
from pathlib import Path

from symbian.analysis import inspect_elf
from symbian.e32 import convert_pic_executable, inspect_image
from symbian.process import run
from symbian.status import Code, StatusError
from symbian.toolchain import FLAGS, _compiler


def _project_file(project: Path, options: dict, field: str) -> Path:
    filename = options.get(field)
    if not isinstance(filename, str) or "\0" in filename:
        raise StatusError(Code.INVALID_ARGUMENT, f"Missing/invalid {field}")
    path = (project / filename).resolve()
    if not path.is_relative_to(project) or not path.is_file():
        raise StatusError(
            Code.INVALID_ARGUMENT, f"{field} must be a file within the project"
        )
    return path


def build_executable(
    project: Path, output: Path, options: dict, compiler: str, linker: str
) -> dict:
    """Links twice and converts a no-import PIC experiment, without an SDK."""
    source = _project_file(project, options, "source")
    startup = _project_file(project, options, "startup")
    script = _project_file(project, options, "linker_script")
    uid3 = options.get("uid3")
    if type(uid3) is not int or not 0xE0000000 <= uid3 <= 0xEFFFFFFF:
        raise StatusError(Code.INVALID_ARGUMENT, "Experimental UID3 required")
    compiler = _compiler(compiler)
    linker = _compiler(linker)
    versions = {
        "compiler": run([compiler, "--version"], cwd=project),
        "linker": run([linker, "--version"], cwd=project),
    }
    inputs = {path: path.read_bytes() for path in (source, startup, script)}
    cpp_flags = [*FLAGS, "-fPIC"]
    asm_flags = ["--target=armv5t-none-eabi", "-marm", "-nostdinc"]
    link_flags = [
        "-m",
        "armelf",
        "--no-undefined",
        "--emit-relocs",
        "--build-id=none",
        "-T",
        str(script),
    ]
    output.mkdir(parents=True, exist_ok=True)
    artifacts = []
    name = options["name"]
    with tempfile.TemporaryDirectory(prefix="e32-", dir=output) as temporary:
        temporary = Path(temporary)
        for index in range(2):
            tree = temporary / str(index)
            tree.mkdir()
            run(
                [compiler, *cpp_flags, "-c", str(source), "-o", "probe.o"],
                cwd=tree,
            )
            run(
                [compiler, *asm_flags, "-c", str(startup), "-o", "startup.o"],
                cwd=tree,
            )
            run(
                [
                    linker,
                    *link_flags,
                    "startup.o",
                    "probe.o",
                    "-o",
                    "image.elf",
                ],
                cwd=tree,
            )
            elf_bytes = (tree / "image.elf").read_bytes()
            image_bytes = convert_pic_executable(elf_bytes, uid3)
            artifacts.append((elf_bytes, image_bytes))
        if artifacts[0] != artifacts[1]:
            raise StatusError(
                Code.DATA_LOSS, "Two isolated ELF/E32 builds differ"
            )
        if any(path.read_bytes() != data for path, data in inputs.items()):
            raise StatusError(
                Code.ABORTED, "Project inputs changed during build"
            )
        for suffix, data in zip(("elf", "exe"), artifacts[0], strict=True):
            staged = temporary / f"result.{suffix}"
            staged.write_bytes(data)
            staged.replace(output / f"{name}.{suffix}")
    commands = []
    for path, flags, filename in (
        (source, cpp_flags, "probe.o"),
        (startup, asm_flags, "startup.o"),
    ):
        commands.append(
            {
                "directory": str(project),
                "file": str(path),
                "arguments": [
                    compiler,
                    *flags,
                    "-c",
                    str(path),
                    "-o",
                    str(output / filename),
                ],
                "output": str(output / filename),
            }
        )
    database = output / "compile_commands.json"
    database.write_text(json.dumps(commands, indent=2) + "\n", encoding="utf-8")
    image = output / f"{name}.exe"
    elf = output / f"{name}.elf"
    report = {
        "schema": "symbian.e32-pic-experiment/v1",
        "artifact_kind": "experimental-e32-executable",
        "artifact": str(image),
        "sha256": hashlib.sha256(artifacts[0][1]).hexdigest(),
        "linked_elf": str(elf),
        "linked_elf_sha256": hashlib.sha256(artifacts[0][0]).hexdigest(),
        "inputs": {
            str(path): hashlib.sha256(data).hexdigest()
            for path, data in inputs.items()
        },
        "compiler": {"path": compiler, "version": versions["compiler"]},
        "linker": {"path": linker, "version": versions["linker"]},
        "cpp_flags": cpp_flags,
        "asm_flags": asm_flags,
        "link_flags": link_flags,
        "elf": inspect_elf(elf),
        "e32": inspect_image(image),
        "reproducible": True,
        "reproducibility_scope": "two builds on this host/toolchain",
        "compile_commands": str(database),
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "limitations": [
            "No SDK/imports, writable data, exports, constructors or packaging",
            "All relocations must be retained by the trusted linker",
            "Hand-written absolute addresses cannot be detected",
            "Direct thread exit skips User::Exit cleanup; no resources allowed",
            "No proof of Belle SVC mapping or target C++/leave compatibility",
        ],
    }
    (output / "report.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report
