"""Native export contracts with CMake/LLVM proxy build orchestration."""

import hashlib
import json
import tempfile
from importlib.resources import files
from pathlib import Path

from symbian.native import require_native
from symbian.process import run
from symbian.status import Code, StatusError
from symbian.toolchain import _compiler
from symbian.toolchain.project_build import dependencies

_CMAKE = """cmake_minimum_required(VERSION 3.28)
project(symbian_import_proxy LANGUAGES CXX ASM)
add_library(proxy_words OBJECT exports.S)
add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/${SYMBIAN_PROXY_SONAME}"
  COMMAND "${CMAKE_LINKER}" -m armelf -shared --hash-style=sysv
    --build-id=none "--soname=${SYMBIAN_PROXY_SONAME}"
    "--version-script=${CMAKE_SOURCE_DIR}/exports.map"
    -T "${CMAKE_SOURCE_DIR}/proxy.ld" $<TARGET_OBJECTS:proxy_words>
    -o "${CMAKE_BINARY_DIR}/${SYMBIAN_PROXY_SONAME}"
  DEPENDS proxy_words exports.map proxy.ld
  COMMAND_EXPAND_LISTS VERBATIM)
add_custom_target(import_proxy ALL
  DEPENDS "${CMAKE_BINARY_DIR}/${SYMBIAN_PROXY_SONAME}")
if(SYMBIAN_SDK_HEADERS)
  add_library(header_probe OBJECT header_probe.cc)
  target_include_directories(header_probe SYSTEM PRIVATE
    "${SYMBIAN_SDK_HEADERS}")
  target_compile_definitions(header_probe PRIVATE
    __GCC32__ __GCCV3__ __EABI__ __EPOC32__ __MARM__ __MARM_ARMV5__)
  add_custom_command(OUTPUT "${CMAKE_BINARY_DIR}/header_probe.elf"
    COMMAND "${CMAKE_LINKER}" -m armelf -shared --no-undefined
      --hash-style=sysv --build-id=none $<TARGET_OBJECTS:header_probe>
      "${CMAKE_BINARY_DIR}/${SYMBIAN_PROXY_SONAME}"
      -o "${CMAKE_BINARY_DIR}/header_probe.elf"
    DEPENDS header_probe "${CMAKE_BINARY_DIR}/${SYMBIAN_PROXY_SONAME}"
    COMMAND_EXPAND_LISTS VERBATIM)
  add_custom_target(sdk_link_probe ALL
    DEPENDS "${CMAKE_BINARY_DIR}/header_probe.elf")
endif()
"""


def _exports(items) -> list[dict]:
    return [
        {
            field: getattr(item, field)
            for field in ("symbol", "ordinal", "data", "absent")
        }
        for item in items
    ]


def inspect_proxy(path: Path) -> dict:
    """Checks the generated ordinal proxy in the native core."""
    with path.open("rb") as stream:
        data = stream.read(2 * 1024 * 1024 + 1)
    info = require_native().inspect_import_proxy(data)
    return {
        "soname": info.soname,
        "target_dll": info.target_dll,
        "exports": _exports(info.exports),
    }


def build_import_proxy(
    definition: Path,
    symbols: list[str],
    target_dll: str,
    output: Path,
    compiler: str = "clang++",
    linker: str = "ld.lld",
    headers: Path | None = None,
) -> dict:
    """Builds a repeatable selected ordinal proxy using native DEF contracts.

    Args:
        definition: Frozen EABI export definition, retained outside the wheel.
        symbols: Exported function names to include without ordinal renumbering.
        target_dll: Plain DLL basename; UID/version decoration is unsupported.
        output: Directory retaining source, primary CMake tree and reports.
        compiler: Host Clang driver with ARM code generation.
        linker: Host LLD ELF driver.
        headers: Optional public kernel/eka/include for a User::Exit link probe.

    Returns:
        Proxy metadata and build evidence. E32/import execution is unverified.
    """
    definition, output = definition.resolve(), output.resolve()
    with definition.open("rb") as stream:
        data = stream.read(8 * 1024 * 1024 + 1)
    if definition.is_relative_to(output):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Keep the DEF outside the generated output tree",
        )
    native = require_native()
    soname = target_dll.removesuffix(".dll") + ".dso"
    sources = native.generate_import_proxy(data, symbols, soname, target_dll)
    if headers is not None:
        headers = headers.resolve()
        if not (headers / "e32std.h").is_file():
            raise StatusError(Code.NOT_FOUND, "SDK headers require e32std.h")
        if "_ZN4User4ExitEi" not in symbols:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Header probe requires User::Exit"
            )
    tools = {
        "compiler": _compiler(compiler),
        "linker": _compiler(linker),
        "cmake": _compiler("cmake"),
        "ninja": _compiler("ninja"),
    }
    versions = {
        name: run([tool, "--version"], cwd=definition.parent)
        for name, tool in tools.items()
    }
    source = output / "source"
    source.mkdir(parents=True, exist_ok=True)
    for filename, contents in {
        "exports.S": sources.assembly,
        "exports.map": sources.version_script,
        "proxy.ld": sources.linker_script,
        "CMakeLists.txt": _CMAKE,
        "header_probe.cc": (
            Path(native.__file__).parent / "sdk/resources/header_probe.cc"
        ).read_text(encoding="utf-8"),
    }.items():
        path = source / filename
        if path.resolve() == definition:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Proxy source would overwrite DEF"
            )
        path.write_text(contents, encoding="utf-8")
    preset = {
        "version": 6,
        "configurePresets": [{"name": "proxy", "generator": "Ninja"}],
    }
    (source / "CMakePresets.json").write_text(json.dumps(preset) + "\n")
    toolchain = Path(
        str(files("symbian.toolchain").joinpath("cmake/armv5t-pic.cmake"))
    )

    def build(tree: Path) -> tuple[bytes, str]:
        configure = run(
            [
                tools["cmake"],
                "--preset",
                "proxy",
                "-S",
                str(source),
                "-B",
                str(tree),
                f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
                f"-DCMAKE_CXX_COMPILER={tools['compiler']}",
                f"-DCMAKE_ASM_COMPILER={tools['compiler']}",
                f"-DCMAKE_LINKER={tools['linker']}",
                f"-DCMAKE_MAKE_PROGRAM={tools['ninja']}",
                f"-DSYMBIAN_PROXY_SONAME={soname}",
                f"-DSYMBIAN_SDK_HEADERS={headers or ''}",
            ],
            cwd=source,
        )
        log = run([tools["cmake"], "--build", str(tree)], cwd=source)
        return (tree / soname).read_bytes(), configure + "\n" + log

    primary = output / "cmake"
    first, first_log = build(primary)
    artifact_name = "header_probe.elf" if headers is not None else soname
    inputs = dependencies(primary, primary / artifact_name, tools["ninja"])
    digests = {
        path: hashlib.sha256(path.read_bytes()).hexdigest() for path in inputs
    }
    with tempfile.TemporaryDirectory(prefix="repeat-", dir=output) as temporary:
        second, second_log = build(Path(temporary))
        repeated_inputs = dependencies(
            Path(temporary), Path(temporary) / artifact_name, tools["ninja"]
        )
        if inputs != repeated_inputs or any(
            hashlib.sha256(path.read_bytes()).hexdigest() != digest
            for path, digest in digests.items()
        ):
            raise StatusError(Code.ABORTED, "Proxy/SDK inputs changed")
        if first != second:
            raise StatusError(Code.DATA_LOSS, "Proxy builds differ")
        if (
            headers is not None
            and (primary / "header_probe.elf").read_bytes()
            != (Path(temporary) / "header_probe.elf").read_bytes()
        ):
            raise StatusError(Code.DATA_LOSS, "SDK ELF link probes differ")
    if definition.read_bytes() != data:
        raise StatusError(Code.ABORTED, "DEF changed during build")
    path = output / soname
    if path.resolve() == definition:
        raise StatusError(Code.INVALID_ARGUMENT, "Proxy would overwrite DEF")
    path.write_bytes(first)
    metadata = inspect_proxy(path)
    if (
        metadata["exports"] != _exports(sources.exports)
        or metadata["target_dll"] != target_dll
    ):
        raise StatusError(
            Code.DATA_LOSS, "Linked proxy changed native export contract"
        )
    report = {
        "schema": "symbian.import-proxy-experiment/v1",
        "artifact": str(path),
        "sha256": hashlib.sha256(first).hexdigest(),
        "definition": str(definition),
        "definition_sha256": hashlib.sha256(data).hexdigest(),
        "exports_in_definition": len(native.parse_def(data)),
        "inputs": {
            str(path): digest for path, digest in sorted(digests.items())
        },
        "proxy": metadata,
        "tools": {
            name: {"path": path, "version": versions[name]}
            for name, path in tools.items()
        },
        "compile_commands": str(primary / "compile_commands.json"),
        "build_logs": {"primary": first_log, "repeated": second_log},
        "reproducible": True,
        "sdk_headers": str(headers) if headers else None,
        "sdk_header_link_probe": (
            str(primary / "header_probe.elf") if headers else None
        ),
        "symbian_loader_verified": False,
        "import_execution_verified": False,
        "limitations": [
            "Ordinal proxy, not an executable target DLL",
            "Public pre-Belle exports/headers are not a verified Nokia 808 SDK",
            "ELF links only; E32 import/GOT conversion and runtime remain open",
        ],
    }
    (output / "report.json").write_text(json.dumps(report, indent=2) + "\n")
    return report
