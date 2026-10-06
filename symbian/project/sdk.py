"""Local target SDK exports with explicit external host dependencies."""

import hashlib
import json
import os
import platform
import shutil
import sys
import tarfile
import tempfile
from importlib.resources import files
from pathlib import Path
from typing import Callable, Literal

from pydantic import BaseModel, ConfigDict, ValidationError

from symbian import toolchain
from symbian.process import run
from symbian.sdk import build_import_proxy
from symbian.status import Code, StatusError
from symbian.toolchain.host_tools import llvm_tool


class AppSdk(BaseModel):
    """Explicit dependency locations of the current bounded SDK profile."""

    model_config = ConfigDict(extra="forbid", frozen=True)

    schema_version: Literal[1] = 1
    prefix: Path
    compiler: Path
    c_compiler: Path | None = None
    linker: Path
    ar: Path | None = None
    ranlib: Path | None = None
    python: Path | None = None
    emulator: Path | None = None
    firmware_importer: Path | None = None
    gdb: Path | None = None
    architectures: tuple[Literal["armv5t", "armv6"], ...] = (
        "armv6",
        "armv5t",
    )

    @classmethod
    def load(cls, path: Path) -> "AppSdk":
        """Loads and validates the materialized SDK's declared dependencies."""
        try:
            sdk = cls.model_validate_json(path.read_text())
            # Distribution paths are relative to the manifest, including the
            # prefix itself. Development exports may retain external paths.
            directory = path.resolve().parent
            updates = {}
            for name in (
                "prefix",
                "compiler",
                "c_compiler",
                "linker",
                "ar",
                "ranlib",
                "python",
                "emulator",
                "firmware_importer",
                "gdb",
            ):
                value = getattr(sdk, name)
                if value is not None and not value.is_absolute():
                    updates[name] = directory / value
            if sdk.python is None:
                updates["python"] = Path(sys.executable).absolute()
            sdk = sdk.model_copy(update=updates)
        except ValidationError as error:
            raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
        for dependency in (
            sdk.compiler,
            sdk.c_compiler,
            sdk.linker,
            sdk.ar,
            sdk.ranlib,
            sdk.python,
            sdk.prefix / "lib/libsymbian_guest_runtime.a",
            sdk.prefix / "cmake/SymbianApp.cmake",
        ):
            if dependency is not None and not dependency.is_file():
                raise StatusError(
                    Code.NOT_FOUND, f"SDK dependency: {dependency}"
                )
        return sdk


def _emulator_binary(workspace: Path) -> Path:
    """Returns the pinned research frontend's expected host build output."""
    system = platform.system()
    if system == "Darwin":
        return workspace / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    if system == "Linux":
        return workspace / "build/eka2l1/bin/eka2l1_qt"
    raise StatusError(Code.UNIMPLEMENTED, f"Unsupported SDK host: {system}")


def _build_runtime_variant(
    workspace: Path,
    architecture: str,
    compiler: Path,
    linker: Path,
    *,
    profile: Literal["default", "streams", "native_atomic64"],
) -> tuple[bytes, bytes]:
    """Builds a complete alternate runtime twice in isolated CMake trees."""
    cmake = shutil.which("cmake")
    ninja = shutil.which("ninja")
    if cmake is None or ninja is None:
        raise StatusError(Code.NOT_FOUND, "CMake and Ninja are required")
    project = workspace / "probes/runtime_probe"
    toolchain_file = workspace / "symbian/toolchain/cmake/symbian-arm.cmake"
    outputs = []
    with tempfile.TemporaryDirectory(
        prefix=f"{profile}-{architecture}-", dir=workspace / ".symbian"
    ) as temporary:
        for index in range(2):
            tree = Path(temporary) / str(index)
            run(
                [
                    cmake,
                    "-S",
                    str(project),
                    "-B",
                    str(tree),
                    "-G",
                    "Ninja",
                    f"-DCMAKE_TOOLCHAIN_FILE={toolchain_file}",
                    f"-DSYMBIAN_PLATFORM_ROOT={workspace}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    f"-DCMAKE_CXX_COMPILER={compiler}",
                    f"-DCMAKE_C_COMPILER={compiler.parent / 'clang'}",
                    f"-DCMAKE_LINKER={linker}",
                    f"-DCMAKE_AR={compiler.parent / 'llvm-ar'}",
                    f"-DCMAKE_RANLIB={compiler.parent / 'llvm-ranlib'}",
                    f"-DCMAKE_MAKE_PROGRAM={ninja}",
                    "-DSYMBIAN_RUNTIME_LOCALE_STREAM="
                    + ("ON" if profile == "streams" else "OFF"),
                    "-DSYMBIAN_RUNTIME_NATIVE_ATOMIC64="
                    + ("ON" if profile == "native_atomic64" else "OFF"),
                    "-DSYMBIAN_RUNTIME_MIMALLOC="
                    + ("OFF" if profile == "native_atomic64" else "ON"),
                    "-DSYMBIAN_IMPORT_PROXIES="
                    + str(workspace / ".symbian/runtime-sdk/euser/euser.dso"),
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                ],
                cwd=project,
                timeout=120,
            )
            run(
                [
                    cmake,
                    "--build",
                    str(tree),
                    "--target",
                    "symbian_guest_runtime",
                    "symbian_header_canaries",
                ],
                cwd=tree,
                timeout=120,
            )
            outputs.append(
                (
                    (tree / "runtime/libsymbian_guest_runtime.a").read_bytes(),
                    (tree / "runtime/include/__config_site").read_bytes(),
                )
            )
    if outputs[0] != outputs[1]:
        raise StatusError(
            Code.DATA_LOSS,
            f"Independent {profile} runtime builds differ",
        )
    return outputs[0]


def _build_abseil(
    workspace: Path, output: Path, compiler: Path, linker: Path
) -> None:
    """Builds the pinned, patched StatusOr closure for both ARM targets."""
    source = workspace / "research/upstream/abseil-cpp"
    revision = "5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a"
    if not (source / "absl/status/statusor.h").is_file():
        raise StatusError(
            Code.NOT_FOUND,
            "Prepare the pinned Abseil checkout at "
            f"{source} before exporting the SDK",
        )
    if run(["git", "rev-parse", "HEAD"], cwd=source) != revision:
        raise StatusError(Code.FAILED_PRECONDITION, "Abseil revision mismatch")
    if run(["git", "status", "--porcelain"], cwd=source):
        raise StatusError(
            Code.FAILED_PRECONDITION, "Abseil source checkout is modified"
        )
    patches = [
        workspace / "research/abseil/symbian-platform.patch",
        workspace / "research/abseil/symbian-low-level-alloc.patch",
        workspace / "research/abseil/symbian-container-no-elf-tls.patch",
    ]
    project = workspace / "probes/abseil_status_probe"
    archives_by_architecture = {}
    with tempfile.TemporaryDirectory(
        prefix="abseil-sdk-", dir=workspace / ".symbian"
    ) as temporary:
        staged = Path(temporary) / "source"
        run(
            ["git", "clone", "--no-hardlinks", str(source), str(staged)],
            cwd=workspace,
            timeout=120,
        )
        for patch in patches:
            run(["git", "apply", "--check", str(patch)], cwd=staged)
            run(["git", "apply", str(patch)], cwd=staged)
            run(
                ["git", "apply", "--reverse", "--check", str(patch)],
                cwd=staged,
            )

        headers = output / "include/abseil"
        for header in (staged / "absl").rglob("*"):
            if header.is_file() and header.suffix in (".h", ".inc"):
                target = headers / header.relative_to(staged)
                target.parent.mkdir(parents=True, exist_ok=True)
                shutil.copyfile(header, target)

        environment = dict(os.environ)
        environment.update(
            SYMBIAN_ABSEIL_SOURCE=str(staged),
            SYMBIAN_SDK_PREFIX=str(output),
        )
        for architecture in ("armv5t", "armv6"):
            build = Path(temporary) / f"build-{architecture}"
            run(
                [
                    "cmake",
                    "--preset",
                    "symbian-pic",
                    "-B",
                    str(build),
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    f"-DCMAKE_CXX_COMPILER={compiler}",
                    f"-DCMAKE_C_COMPILER={compiler.parent / 'clang'}",
                    f"-DCMAKE_LINKER={linker}",
                    f"-DCMAKE_AR={compiler.parent / 'llvm-ar'}",
                    f"-DCMAKE_RANLIB={compiler.parent / 'llvm-ranlib'}",
                ],
                cwd=project,
                env=environment,
                timeout=120,
            )
            run(
                [
                    "cmake",
                    "--build",
                    str(build),
                    "--target",
                    "abseil_status_probe",
                    "-j",
                    "8",
                ],
                cwd=project,
                env=environment,
                timeout=600,
            )
            artifacts = sorted((build / "abseil").rglob("libabsl_*.a"))
            if not any(path.name == "libabsl_statusor.a" for path in artifacts):
                raise StatusError(
                    Code.DATA_LOSS,
                    f"Abseil StatusOr archive missing for {architecture}",
                )
            destination = output / "lib" / architecture / "abseil"
            destination.mkdir(parents=True)
            names = set()
            for artifact in artifacts:
                target = destination / artifact.name
                if target.exists():
                    raise StatusError(
                        Code.DATA_LOSS,
                        f"Duplicate Abseil archive: {artifact.name}",
                    )
                shutil.copyfile(artifact, target)
                names.add(target.name)
            archives_by_architecture[architecture] = names
    if archives_by_architecture["armv5t"] != archives_by_architecture["armv6"]:
        raise StatusError(
            Code.DATA_LOSS, "Abseil archive closure differs by architecture"
        )


def _export_mbedtls_source(source: Path, output: Path) -> None:
    """Copy the vendored port sources without Git, caches or build products."""
    excluded = {
        ".git",
        ".idea",
        ".pytest_cache",
        ".ruff_cache",
        ".symbian",
        "__pycache__",
        "build",
        "install",
    }
    relative_paths = sorted(
        path.relative_to(source)
        for path in source.rglob("*")
        if path.is_file()
        and not any(part in excluded for part in path.relative_to(source).parts)
    )
    destination = output / "source/mbedtls-symbian"
    for relative in relative_paths:
        original = source / relative
        if not original.is_file() or not original.resolve().is_relative_to(
            source
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION,
                "Mbed TLS source input is missing or escapes vendor tree: "
                f"{relative}",
            )
        target = destination / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(original, target)
        content = target.read_bytes()
        if content != original.read_bytes():
            raise StatusError(
                Code.ABORTED, "Mbed TLS source changed during export"
            )


def _build_mbedtls(
    workspace: Path,
    output: Path,
    compiler: Path,
    linker: Path,
    archive_tools: dict[str, Path],
) -> Path:
    """Installs architecture-specific static TLS packages into the SDK.

    The vendored port has its own CMake targets. An application links TLS only
    when it explicitly requests a target.
    """
    source = (workspace / "third_party/mbedtls-symbian").resolve()
    if (
        not (source / "CMakeLists.txt").is_file()
        or not (source / "include/symbian_mbedtls/config.h").is_file()
    ):
        raise StatusError(
            Code.NOT_FOUND,
            "Default SDK requires vendored third_party/mbedtls-symbian",
        )
    cmake_tool = shutil.which("cmake")
    ninja_tool = shutil.which("ninja")
    if cmake_tool is None or ninja_tool is None:
        raise StatusError(Code.NOT_FOUND, "CMake and Ninja are required")
    for architecture in ("armv5t", "armv6"):
        with tempfile.TemporaryDirectory(
            prefix=f"symbian-mbedtls-{architecture}-"
        ) as temporary:
            build = Path(temporary) / "build"
            package = output / "lib" / architecture / "mbedtls"
            run(
                [
                    cmake_tool,
                    "-S",
                    str(source),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    f"-DSYMBIAN_SDK_PREFIX={output}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    f"-DCMAKE_C_COMPILER={compiler.parent / 'clang'}",
                    f"-DCMAKE_CXX_COMPILER={compiler}",
                    f"-DCMAKE_LINKER={linker}",
                    f"-DCMAKE_AR={archive_tools['llvm-ar']}",
                    f"-DCMAKE_RANLIB={archive_tools['llvm-ranlib']}",
                    f"-DCMAKE_MAKE_PROGRAM={ninja_tool}",
                    "-DCMAKE_BUILD_TYPE=Release",
                    "-DSYMBIAN_MBEDTLS_TLS13=ON",
                    "-DSYMBIAN_MBEDTLS_OPENSSL_COMPAT=OFF",
                    "-DSYMBIAN_MBEDTLS_TESTS=OFF",
                    "-DSYMBIAN_MBEDTLS_GUEST_PROBE=OFF",
                    "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                ],
                cwd=workspace,
                timeout=120,
            )
            run(
                [cmake_tool, "--build", str(build), "-j", "6"],
                cwd=workspace,
                timeout=600,
            )
            run(
                [cmake_tool, "--install", str(build), "--prefix", str(package)],
                cwd=workspace,
                timeout=120,
            )
            names = ("libmbedcrypto.a", "libmbedx509.a", "libmbedtls.a")
            if not all((package / "lib" / name).is_file() for name in names):
                raise StatusError(
                    Code.DATA_LOSS,
                    f"Incomplete Mbed TLS package for {architecture}",
                )
    # Keep the public C headers at the ordinary SDK include root as well as in
    # the relocatable CMake packages for direct, non-CMake consumers.
    shutil.copytree(
        output / "lib/armv6/mbedtls/include",
        output / "include",
        dirs_exist_ok=True,
    )
    _export_mbedtls_source(source, output)
    shutil.copytree(
        workspace / "cpp/symbian/entropy",
        output / "source/mbedtls-symbian/sdk_entropy",
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("CMakeLists.txt"),
    )
    return source


def stage_headers(workspace: Path, output: Path, compiler: Path) -> None:
    """Stages shared platform and SDK headers for exports and source builds."""
    from symbian.project.graphics import stage_graphics_headers

    stage_graphics_headers(workspace, output)
    source = workspace / "research/upstream"
    libcxx = source / "llvm-project/libcxx"
    openc = source / "ossrv/genericopenlibs/openenvcore"
    mimalloc = source / "mimalloc"
    shutil.copytree(
        workspace / ".symbian/gui-sdk/include", output / "include/platform"
    )
    for header in ("hal.h", "hal_data.h"):
        shutil.copyfile(
            workspace
            / "research/upstream/kernelhwsrv/halservices/hal/inc"
            / header,
            output / "include/platform" / header,
        )
    for header in ("e32math.h", "e32math.inl"):
        shutil.copyfile(
            source / "kernelhwsrv/kernel/eka/include" / header,
            output / "include/platform" / header,
        )
    camera_headers = (
        "ECam.h",
        "ecamdef.h",
        "ecamconst.h",
        "ECamUids.hrh",
        "ecamuidsconst.hrh",
        "ecamuidsdef.hrh",
    )
    camera_source = (
        workspace / "research/upstream/mm/imagingandcamerafws/camerafw/Include"
    )
    for header in camera_headers:
        shutil.copyfile(
            camera_source / header,
            output / "include/platform" / header,
        )
    # ECam.h names this include in lower case; preserve it on case-sensitive
    # SDK hosts as well as the upstream-cased original file.
    shutil.copyfile(
        camera_source / "ECamUids.hrh",
        output / "include/platform/ecamuids.hrh",
    )
    shutil.copyfile(
        workspace
        / "research/upstream/appsupport/appfw/apparchitecture/inc"
        / "AppInfo.rh",
        output / "include/platform/AppInfo.rh",
    )
    shutil.copytree(libcxx / "include", output / "include/c++")
    resource = Path(
        run([str(compiler), "-print-resource-dir"], cwd=workspace).strip()
    )
    shutil.copytree(resource / "include", output / "include/compiler")
    shutil.copytree(openc / "include", output / "include/openc")
    shutil.copytree(openc / "libm/include", output / "include/libm")
    shutil.copytree(openc / "libc/inc", output / "include/libc")
    shutil.copytree(openc / "libpthread/inc", output / "include/pthread")
    shutil.copytree(openc / "include/posix4", output / "include/posix4")
    shutil.copytree(
        mimalloc / "include", output / "include", dirs_exist_ok=True
    )
    shutil.copytree(
        workspace / "third_party/symbian-network-headers/include",
        output / "include/platform",
        dirs_exist_ok=True,
    )
    from symbian.project.native_surface import stage_native_headers

    stage_native_headers(workspace, output)
    startup = output / "share/symbian/runtime"
    startup.mkdir(parents=True)
    for source_name, target_name in (
        ("exe_startup.S", "startup.S"),
        ("exe_startup.cc", "startup.cc"),
        ("exe_image.ld", "image.ld"),
    ):
        shutil.copyfile(
            workspace / "symbian/toolchain/cmake" / source_name,
            startup / target_name,
        )
    (output / "include/symbian").mkdir()
    shutil.copyfile(
        workspace / "cpp/symbian/runtime/abi.h",
        output / "include/symbian/runtime.h",
    )
    shutil.copytree(
        workspace / "cpp/symbian/concurrency/common/symbian",
        output / "include/symbian",
        dirs_exist_ok=True,
    )
    shutil.copytree(
        workspace / "cpp/symbian/concurrency/guest/symbian",
        output / "include/symbian",
        dirs_exist_ok=True,
    )
    shutil.copytree(
        workspace / "cpp/symbian/api/include/symbian",
        output / "include/symbian",
        dirs_exist_ok=True,
    )
    for component in ("http", "net"):
        destination = output / "include/symbian" / component
        destination.mkdir()
        for header in (workspace / "cpp/symbian" / component).glob("*.h"):
            shutil.copyfile(header, destination / header.name)
    (output / "include/symbian/websocket").mkdir()
    shutil.copyfile(
        workspace / "cpp/symbian/websocket/websocket.h",
        output / "include/symbian/websocket/websocket.h",
    )
    (output / "include/symbian/agent").mkdir()
    shutil.copyfile(
        workspace / "cpp/symbian/agent/guest_control.h",
        output / "include/symbian/agent/guest_control.h",
    )
    shutil.copyfile(
        workspace / "cpp/symbian/agent/guest_log.h",
        output / "include/symbian/agent/guest_log.h",
    )
    shutil.copyfile(
        workspace / "cpp/symbian/agent/guest_files.h",
        output / "include/symbian/agent/guest_files.h",
    )
    shutil.copytree(
        workspace / "cpp/symbian/concurrency/common/thread",
        output / "include/thread",
    )
    shutil.copytree(
        workspace / "cpp/symbian/concurrency/guest/thread",
        output / "include/thread",
        dirs_exist_ok=True,
        ignore=shutil.ignore_patterns("*.cc"),
    )
    host_headers = output / "include/host/thread"
    host_headers.mkdir(parents=True)
    for header in (workspace / "cpp/symbian/concurrency/host/thread").glob(
        "*.h"
    ):
        shutil.copyfile(header, host_headers / header.name)


def stage_imports(
    workspace: Path,
    output: Path,
    compiler: Path,
    linker: Path,
    progress: Callable[[str], None] | None = None,
) -> None:
    """Builds complete OS and Qt import interfaces once for SDK consumers."""
    from symbian.project.graphics import stage_graphics_imports

    if progress:
        progress("building Khronos frozen imports")
    stage_graphics_imports(workspace, output, compiler, linker)
    if progress:
        progress("building core OS frozen imports")
    source = workspace / "research/upstream"
    # Every original library target owns its complete frozen ordinal ABI.
    for dll, definition in (
        ("euser", source / "kernelhwsrv/kernel/eka/eabi/euseru.def"),
        (
            "ws32",
            source / "graphics/windowing/windowserver/eabi/WS322U.DEF",
        ),
        (
            "gdi",
            source / "graphics/graphicsdeviceinterface/gdi/eabi/GDI2U.def",
        ),
        ("hal", source / "kernelhwsrv/halservices/hal/eabi/halu.def"),
        ("ecam", source / "mm/imagingandcamerafws/camerafw/eabi/ecamU.def"),
        (
            "efsrv",
            source
            / "kernelhwsrv/userlibandfileserver/fileserver/eabi/efsrvu.def",
        ),
        (
            "esock",
            workspace / "third_party/symbian-network-headers/esockU.def",
        ),
        (
            "insock",
            workspace / "third_party/symbian-network-headers/insockU.def",
        ),
        (
            "libc",
            source / "ossrv/genericopenlibs/openenvcore/libc/eabi/libcu.def",
        ),
        (
            "libm",
            source / "ossrv/genericopenlibs/openenvcore/libm/eabi/libmu.def",
        ),
        (
            "libpthread",
            source
            / "ossrv/genericopenlibs/openenvcore/libpthread/eabi"
            / "libpthreadu.def",
        ),
        (
            "drtaeabi",
            source / "kernelhwsrv/kernel/eka/compsupp/eabi/drtaeabiu.def",
        ),
    ):
        build_import_proxy(
            definition,
            [],
            dll + ".dll",
            output / "proxies" / dll,
            str(compiler),
            str(linker),
        )
    shutil.copytree(output / "proxies/euser", output / "proxies/euser-native64")
    from symbian.project.qt import prepare_qt

    build_import_proxy(
        workspace / "symbian/toolchain/cmake/euser_eka1.def",
        [],
        "euser.dll",
        output / "proxies/euser-eka1",
        str(compiler),
        str(linker),
    )

    prepare_qt(
        workspace / "research/upstream/qt4",
        output,
        str(compiler),
        str(linker),
    )
    if progress:
        progress("Qt 4 public modules staged; building Qt Mobility imports")
    from symbian.project.qtmobility import prepare_qt_mobility

    prepare_qt_mobility(workspace, output, str(compiler), str(linker))
    if progress:
        progress("Qt Mobility staged; building base-platform imports")
    from symbian.project.native_surface import stage_native_imports

    stage_native_imports(workspace, output, compiler, linker, progress=progress)
    if progress:
        progress("all frozen imports staged and validated")


def prepare(
    workspace: Path, output: Path, *, include_host: bool = True
) -> AppSdk:
    """Exports target headers/runtime/proxies without source-tree symlinks.

    Args:
        workspace: Prepared platform source checkout with preserved inputs.
        output: New SDK prefix, outside the upstream research source trees.
        include_host: Build the development host primitives too. Release builds
            reuse their independently built host SDK instead.

    Returns:
        SDK dependency manifest. LLVM, GDB and the emulator are external host
        dependencies in this development export, not bundled wheel payloads.
    """
    workspace, output = workspace.resolve(), output.absolute()
    if any(
        output.resolve().is_relative_to(workspace / name)
        for name in ("research", "cpp", "symbian", "examples", "third_party")
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "SDK output overlaps source inputs"
        )
    if output.exists():
        raise StatusError(Code.ALREADY_EXISTS, f"SDK prefix exists: {output}")
    compiler = llvm_tool("clang++")
    c_compiler = llvm_tool("clang", sibling=compiler.parent)
    linker = llvm_tool("ld.lld", sibling=compiler.parent)
    mimalloc = workspace / "research/upstream/mimalloc"
    mimalloc_revision = "d4881d338125e1cb7c47ba4cfb398d6f7c0c8d45"
    if not (mimalloc / "include/mimalloc.h").is_file():
        raise StatusError(
            Code.NOT_FOUND,
            f"Prepare the pinned mimalloc checkout at {mimalloc}",
        )
    if run(["git", "rev-parse", "HEAD"], cwd=mimalloc) != mimalloc_revision:
        raise StatusError(
            Code.FAILED_PRECONDITION, "Mimalloc revision mismatch"
        )
    if run(["git", "status", "--porcelain"], cwd=mimalloc):
        raise StatusError(
            Code.FAILED_PRECONDITION, "Mimalloc source checkout is modified"
        )
    nghttp2_source = Path(
        os.environ.get(
            "SYMBIAN_NGHTTP2_SOURCE", str(workspace / "third_party/nghttp2")
        )
    ).resolve()
    if not (nghttp2_source / "COPYING").is_file():
        raise StatusError(
            Code.NOT_FOUND, "SDK export requires pinned nghttp2 source"
        )
    nghttp2_manifest = json.loads(
        (workspace / "cpp/symbian/net/nghttp2-source.json").read_text()
    )
    for relative, expected in nghttp2_manifest["files"].items():
        source_input = nghttp2_source / relative
        if (
            not source_input.is_file()
            or hashlib.sha256(source_input.read_bytes()).hexdigest() != expected
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION,
                f"nghttp2 release input mismatch: {relative}",
            )
    archive_tools = {}
    for name in ("llvm-ar", "llvm-ranlib"):
        archive_tools[name] = llvm_tool(name, sibling=compiler.parent)
    runtimes = {}
    for architecture in ("armv5t", "armv6"):
        runtime = workspace / f".symbian/runtime-probe-{architecture}"
        toolchain.build(
            workspace / "probes/runtime_probe",
            runtime,
            str(compiler),
            str(linker),
            architecture=architecture,
            cmake_variables={
                "SYMBIAN_IMPORT_PROXIES": str(
                    workspace / ".symbian/runtime-sdk/euser/euser.dso"
                ),
            },
        )
        runtimes[architecture] = runtime
    default_runtimes = {
        architecture: _build_runtime_variant(
            workspace, architecture, compiler, linker, profile="default"
        )
        for architecture in ("armv5t", "armv6")
    }
    stream_runtimes = {
        architecture: _build_runtime_variant(
            workspace, architecture, compiler, linker, profile="streams"
        )
        for architecture in ("armv5t", "armv6")
    }
    native_atomic64_runtimes = {
        architecture: _build_runtime_variant(
            workspace,
            architecture,
            compiler,
            linker,
            profile="native_atomic64",
        )
        for architecture in ("armv5t", "armv6")
    }
    runtime = runtimes[
        "armv5t"
    ]  # Compatibility archive for older project files.
    source = workspace / "research/upstream"
    libcxx = source / "llvm-project/libcxx"
    output.mkdir(parents=True)
    try:
        stage_headers(workspace, output, compiler)
        # __config_site and assertion handler are generated with the library;
        # consumers must use these exact files, not host libc++ configuration.
        config = runtime / "cmake/runtime/include"
        (output / "include/config").mkdir()
        for name in ("__config_site", "__assertion_handler"):
            shutil.copyfile(config / name, output / "include/config" / name)
        shutil.copyfile(
            workspace / "cpp/symbian/runtime/include/stdarg_e.h",
            output / "include/config/stdarg_e.h",
        )
        shutil.copyfile(
            workspace / "cpp/symbian/runtime/include/ctype.h",
            output / "include/config/ctype.h",
        )
        shutil.copyfile(
            workspace / "cpp/symbian/runtime/include/inttypes.h",
            output / "include/config/inttypes.h",
        )
        shutil.copyfile(
            workspace / "cpp/symbian/runtime/include/math.h",
            output / "include/config/math.h",
        )
        locale_header = output / "include/config/__locale_dir"
        locale_header.mkdir()
        shutil.copyfile(
            workspace
            / "cpp/symbian/runtime/include/__locale_dir/locale_base_api.h",
            locale_header / "locale_base_api.h",
        )
        stream_config = output / "include/stream-config"
        stream_config.mkdir()
        stream_config.joinpath("__config_site").write_bytes(
            stream_runtimes["armv6"][1]
        )
        (output / "include/config/stdapis").symlink_to(
            "../openc", target_is_directory=True
        )
        (output / "lib").mkdir()
        for architecture, built in runtimes.items():
            library = output / "lib" / architecture
            library.mkdir()
            library.joinpath("libsymbian_guest_runtime.a").write_bytes(
                default_runtimes[architecture][0]
            )
            library.joinpath("libsymbian_guest_runtime_streams.a").write_bytes(
                stream_runtimes[architecture][0]
            )
            library.joinpath(
                "libsymbian_guest_runtime_native_atomic64.a"
            ).write_bytes(native_atomic64_runtimes[architecture][0])
            if stream_runtimes[architecture][1] != stream_runtimes["armv6"][1]:
                raise StatusError(
                    Code.DATA_LOSS,
                    "Stream runtime configurations differ across ARM targets",
                )
            if (
                native_atomic64_runtimes[architecture][1]
                != (config / "__config_site").read_bytes()
            ):
                raise StatusError(
                    Code.DATA_LOSS,
                    "Native atomic runtime configuration differs from default",
                )
            if (
                default_runtimes[architecture][1]
                != (config / "__config_site").read_bytes()
            ):
                raise StatusError(
                    Code.DATA_LOSS, "Default runtime configurations differ"
                )
            for name in ("__config_site", "__assertion_handler"):
                if (built / "cmake/runtime/include" / name).read_bytes() != (
                    config / name
                ).read_bytes():
                    raise StatusError(
                        Code.DATA_LOSS,
                        "Target runtime header configurations differ",
                    )
        (output / "lib/libsymbian_guest_runtime.a").write_bytes(
            default_runtimes["armv5t"][0]
        )
        stage_imports(workspace, output, compiler, linker)
        cmake = output / "cmake"
        shutil.copytree(workspace / "symbian/toolchain/cmake", cmake)
        shutil.copyfile(
            workspace / "cmake/SymbianHeaderCanary.cmake",
            cmake / "SymbianHeaderCanary.cmake",
        )
        (cmake / "SymbianApp.cmake").write_bytes(
            files("symbian.project")
            .joinpath("templates", "SymbianApp.cmake")
            .read_bytes()
        )
        (cmake / "SymbianAbseil.cmake").write_bytes(
            files("symbian.project")
            .joinpath("templates", "SymbianAbseil.cmake")
            .read_bytes()
        )
        (cmake / "SymbianHostConcurrency.cmake").write_bytes(
            files("symbian.project")
            .joinpath("templates", "SymbianHostConcurrency.cmake")
            .read_bytes()
        )
        _build_abseil(workspace, output, compiler, linker)
        cmake_tool = shutil.which("cmake")
        ninja_tool = shutil.which("ninja")
        if cmake_tool is None or ninja_tool is None:
            raise StatusError(Code.NOT_FOUND, "CMake and Ninja are required")
        for architecture in ("armv5t", "armv6"):
            with tempfile.TemporaryDirectory(
                prefix=f"symbian-fiber-{architecture}-"
            ) as temporary:
                build_tree = Path(temporary) / "build"
                run(
                    [
                        cmake_tool,
                        "-S",
                        str(workspace / "cpp/symbian/concurrency/guest"),
                        "-B",
                        str(build_tree),
                        "-G",
                        "Ninja",
                        f"-DSYMBIAN_SDK_PREFIX={output}",
                        f"-DSYMBIAN_TARGET_ARCH={architecture}",
                        "-DCMAKE_TOOLCHAIN_FILE="
                        f"{output / 'cmake/symbian-arm.cmake'}",
                        f"-DCMAKE_CXX_COMPILER={compiler}",
                        f"-DCMAKE_LINKER={linker}",
                        f"-DCMAKE_AR={archive_tools['llvm-ar']}",
                        f"-DCMAKE_RANLIB={archive_tools['llvm-ranlib']}",
                        f"-DCMAKE_MAKE_PROGRAM={ninja_tool}",
                    ],
                    cwd=workspace,
                )
                run(
                    [cmake_tool, "--build", str(build_tree)],
                    cwd=workspace,
                )
                shutil.copyfile(
                    build_tree / "libsymbian_guest_fiber.a",
                    output / "lib" / architecture / "libsymbian_guest_fiber.a",
                )
        for architecture in ("armv5t", "armv6"):
            with tempfile.TemporaryDirectory(
                prefix=f"symbian-device-api-{architecture}-"
            ) as temporary:
                build_tree = Path(temporary) / "build"
                run(
                    [
                        cmake_tool,
                        "-S",
                        str(workspace / "cpp/symbian/api"),
                        "-DSYMBIAN_NGHTTP2_SOURCE=" + str(nghttp2_source),
                        "-B",
                        str(build_tree),
                        "-G",
                        "Ninja",
                        f"-DSYMBIAN_SDK_PREFIX={output}",
                        f"-DSYMBIAN_TARGET_ARCH={architecture}",
                        "-DCMAKE_TOOLCHAIN_FILE="
                        f"{output / 'cmake/symbian-arm.cmake'}",
                        f"-DCMAKE_CXX_COMPILER={compiler}",
                        f"-DCMAKE_LINKER={linker}",
                        f"-DCMAKE_AR={archive_tools['llvm-ar']}",
                        f"-DCMAKE_RANLIB={archive_tools['llvm-ranlib']}",
                        f"-DCMAKE_MAKE_PROGRAM={ninja_tool}",
                        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    ],
                    cwd=workspace,
                )
                for component in (
                    "system",
                    "connectivity",
                    "websocket",
                    "agent",
                    "power",
                    "display",
                    "storage",
                    "camera",
                ):
                    target = f"symbian_api_{component}"
                    run(
                        [
                            cmake_tool,
                            "--build",
                            str(build_tree),
                            "--target",
                            target,
                        ],
                        cwd=workspace,
                    )
                    shutil.copyfile(
                        build_tree
                        / (
                            "connectivity"
                            if component == "websocket"
                            else component
                        )
                        / f"lib{target}.a",
                        output / "lib" / architecture / f"lib{target}.a",
                    )
                for component, library in (
                    ("websocket", "symbian_websocket"),
                    ("http", "symbian_http"),
                    ("net", "symbian_nghttp2"),
                ):
                    shutil.copyfile(
                        build_tree / component / f"lib{library}.a",
                        output / "lib" / architecture / f"lib{library}.a",
                    )
        if include_host:
            with tempfile.TemporaryDirectory(
                prefix="symbian-host-concurrency-"
            ) as temporary:
                build_tree = Path(temporary) / "build"
                configure = [
                    cmake_tool,
                    "-S",
                    str(workspace / "cpp/symbian/concurrency/host_package"),
                    "-B",
                    str(build_tree),
                    "-G",
                    "Ninja",
                    f"-DCMAKE_MAKE_PROGRAM={ninja_tool}",
                ]
                deps_prefix = os.environ.get("SYMBIAN_DEPS_PREFIX")
                if deps_prefix:
                    configure.append(f"-DCMAKE_PREFIX_PATH={deps_prefix}")
                run(configure, cwd=workspace, timeout=120)
                run(
                    [
                        cmake_tool,
                        "--build",
                        str(build_tree),
                        "--target",
                        "symbian_host_primitives_bundle",
                    ],
                    cwd=workspace,
                    timeout=300,
                )
                host_library = (
                    output
                    / "lib/host"
                    / f"{platform.system()}-{platform.machine()}"
                )
                host_library.mkdir(parents=True)
                shutil.copyfile(
                    build_tree / "concurrency/libsymbian_host_primitives.a",
                    host_library / "libsymbian_host_primitives.a",
                )
        mbedtls_source = _build_mbedtls(
            workspace, output, compiler, linker, archive_tools
        )
        for architecture in ("armv5t", "armv6"):
            with tempfile.TemporaryDirectory(
                prefix=f"symbian-guest-tls-{architecture}-"
            ) as temporary:
                build_tree = Path(temporary) / "build"
                run(
                    [
                        cmake_tool,
                        "-S",
                        str(workspace / "cpp/symbian/tls"),
                        "-B",
                        str(build_tree),
                        "-G",
                        "Ninja",
                        f"-DSYMBIAN_SDK_PREFIX={output}",
                        f"-DSYMBIAN_TARGET_ARCH={architecture}",
                        "-DCMAKE_TOOLCHAIN_FILE="
                        f"{output / 'cmake/symbian-arm.cmake'}",
                        f"-DCMAKE_CXX_COMPILER={compiler}",
                        f"-DCMAKE_LINKER={linker}",
                        f"-DCMAKE_AR={archive_tools['llvm-ar']}",
                        f"-DCMAKE_RANLIB={archive_tools['llvm-ranlib']}",
                        f"-DCMAKE_MAKE_PROGRAM={ninja_tool}",
                        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    ],
                    cwd=workspace,
                )
                run(
                    [
                        cmake_tool,
                        "--build",
                        str(build_tree),
                        "--target",
                        "symbian_api_tls",
                    ],
                    cwd=workspace,
                )
                shutil.copyfile(
                    build_tree / "libsymbian_api_tls.a",
                    output / "lib" / architecture / "libsymbian_api_tls.a",
                )
        for architecture in ("armv5t", "armv6"):
            with tempfile.TemporaryDirectory(
                prefix=f"symbian-headers-{architecture}-"
            ) as temporary:
                project = Path(temporary)
                (project / "CMakeLists.txt").write_text(
                    "cmake_minimum_required(VERSION 3.28)\n"
                    "project(sdk_header_canaries LANGUAGES C CXX)\n"
                    "include(SymbianSdkHeaderCanaries)\n"
                    "symbian_sdk_header_canaries()\n"
                )
                build_tree = project / "build"
                run(
                    [
                        cmake_tool,
                        "-S",
                        str(project),
                        "-B",
                        str(build_tree),
                        "-G",
                        "Ninja",
                        f"-DSYMBIAN_SDK_PREFIX={output}",
                        f"-DSYMBIAN_TARGET_ARCH={architecture}",
                        "-DCMAKE_TOOLCHAIN_FILE="
                        f"{output / 'cmake/symbian-arm.cmake'}",
                        f"-DCMAKE_CXX_COMPILER={compiler}",
                        "-DCMAKE_EXPORT_COMPILE_COMMANDS=ON",
                    ],
                    cwd=workspace,
                )
                run(
                    [cmake_tool, "--build", str(build_tree), "-j", "6"],
                    cwd=workspace,
                )
        licenses = output / "licenses"
        licenses.mkdir(exist_ok=True)
        shutil.copyfile(
            nghttp2_source / "COPYING", licenses / "nghttp2-LICENSE"
        )
        shutil.copyfile(
            mbedtls_source / "LICENSE",
            licenses / "MbedTLS-Apache-2.0.txt",
        )
        shutil.copyfile(libcxx / "LICENSE.TXT", licenses / "LLVM-libcxx.txt")
        shutil.copyfile(
            source / "llvm-project/LICENSE.TXT", licenses / "LLVM-tools.txt"
        )
        shutil.copyfile(
            workspace / "research/upstream/abseil-cpp/LICENSE",
            licenses / "Abseil-Apache-2.0.txt",
        )
        shutil.copyfile(
            workspace / "third_party/a11/LICENSE", licenses / "A11.txt"
        )
        shutil.copyfile(
            workspace / "third_party/boost/LICENSE_1_0.txt",
            licenses / "Boost-BSL-1.0.txt",
        )
        shutil.copyfile(mimalloc / "LICENSE", licenses / "mimalloc-MIT.txt")
        shutil.copyfile(
            source / "llvm-project/compiler-rt/LICENSE.TXT",
            licenses / "LLVM-compiler-rt.txt",
        )
        shutil.copyfile(
            workspace / "third_party/symbian/EPL-1.0.html",
            licenses / "EPL-1.0.html",
        )
        for tree in (
            "kernelhwsrv",
            "graphics",
            "ossrv",
            "persistentdata",
            "textandloc",
        ):
            candidates = list((source / tree).glob("*icense*")) + list(
                (source / tree).glob("*ICENSE*")
            )
            for index, candidate in enumerate(dict.fromkeys(candidates)):
                if candidate.is_file():
                    shutil.copyfile(candidate, licenses / f"{tree}-{index}.txt")
        emulator_binary = _emulator_binary(workspace)
        guest_gdb = shutil.which("arm-none-eabi-gdb") or shutil.which(
            "gdb-multiarch"
        )
        sdk = AppSdk(
            prefix=output,
            architectures=("armv5t", "armv6"),
            compiler=compiler,
            c_compiler=c_compiler,
            linker=linker,
            ar=archive_tools["llvm-ar"],
            ranlib=archive_tools["llvm-ranlib"],
            python=Path(sys.executable).absolute(),
            emulator=emulator_binary,
            firmware_importer=workspace
            / "build/eka2l1/platform-control/symbian_firmware_tool",
            gdb=Path(guest_gdb) if guest_gdb else None,
        )
        (output / "sdk.json").write_text(sdk.model_dump_json(indent=2) + "\n")
        return AppSdk.load(output / "sdk.json")
    except BaseException:
        shutil.rmtree(output)
        raise


def discover_sdk(explicit: Path | None = None) -> Path:
    """Resolves explicit SDK, environment override, then the active user SDK."""
    if explicit is not None:
        return (
            explicit / "sdk.json" if explicit.is_dir() else explicit
        ).absolute()
    if value := os.environ.get("SYMBIAN_SDK_MANIFEST"):
        return Path(value).absolute()
    from symbian.paths import asset_directory

    active = asset_directory("config") / "active-sdk.json"
    if active.is_file():
        return Path(json.loads(active.read_text())["manifest"])
    raise StatusError(
        Code.FAILED_PRECONDITION,
        "Install a local SDK with symbian sdk install or pass --sdk",
    )


def project_sdk(project: Path) -> AppSdk:
    """Loads an SDK for generated projects or standalone source examples."""
    location = project / "sdk-location.json"
    if not location.is_file():
        return AppSdk.load(discover_sdk())
    try:
        prefix = Path(json.loads(location.read_text())["sdk"])
    except (ValueError, KeyError, TypeError) as error:
        raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
    if not prefix.is_absolute():
        prefix = (project / prefix).resolve()
    return AppSdk.load(prefix / "sdk.json")


def activate_sdk(sdk: AppSdk) -> None:
    """Records the active SDK for CLI use from arbitrary directories."""
    from symbian.paths import asset_directory

    active = asset_directory("config") / "active-sdk.json"
    active.parent.mkdir(parents=True, exist_ok=True)
    temporary = active.with_suffix(".tmp")
    temporary.write_text(
        json.dumps({"manifest": str(sdk.prefix / "sdk.json")}) + "\n"
    )
    temporary.replace(active)


def _install_resource_tools(
    bin_path: Path, workspace: Path | None = None
) -> None:
    """Builds the pinned EPL resource compiler and its UID helper."""
    workspace = (workspace or Path(__file__).resolve().parents[2]).resolve()
    source = workspace / ".symbian/rcomp-epl-research"
    support = workspace / "research/rcomp"
    if not (source / "bintools/rcomp/src/main.cpp").is_file():
        raise StatusError(
            Code.NOT_FOUND,
            "Prepared EPL rcomp checkout is required for SDK export; "
            "see .dev/research/rcomp.md",
        )
    revision = run(
        ["git", "-C", str(source), "rev-parse", "HEAD"], cwd=workspace
    ).strip()
    if revision != "d3c2eadd3ff7826bdf9e1d92f447c357571af18b":
        raise StatusError(
            Code.FAILED_PRECONDITION, "Unexpected rcomp source pin"
        )
    for name in ("modern-host.patch", "uidcrc-spawn.patch"):
        patch = support / name
        try:
            run(
                [
                    "git",
                    "-C",
                    str(source),
                    "apply",
                    "--reverse",
                    "--check",
                    str(patch),
                ],
                cwd=workspace,
            )
        except StatusError:
            run(
                ["git", "-C", str(source), "apply", "--check", str(patch)],
                cwd=workspace,
            )
            run(["git", "-C", str(source), "apply", str(patch)], cwd=workspace)
    compiler = shutil.which("clang++") or shutil.which("g++")
    if compiler is None:
        raise StatusError(Code.NOT_FOUND, "Host C++ compiler for rcomp")
    rcomp = source / "bintools/rcomp"
    inputs = sorted((rcomp / "src").glob("*.cpp")) + sorted(
        (rcomp / "src").glob("*.CPP")
    )
    run(
        [
            compiler,
            "-std=c++14",
            "-D__LINUX__",
            "-Wno-deprecated-declarations",
            f"-I{support / 'compat'}",
            f"-I{rcomp / 'inc'}",
            f"-I{rcomp / 'src'}",
            *(str(path) for path in inputs),
            "-o",
            str(bin_path / "rcomp"),
        ],
        cwd=workspace,
        timeout=120,
    )
    run(
        [
            compiler,
            "-std=c++20",
            "-fno-exceptions",
            f"-I{workspace / 'cpp'}",
            f"-I{bin_path.parent / 'include/abseil'}",
            str(workspace / "cpp/symbian/resource/uidcrc_main.cc"),
            "-o",
            str(bin_path / "uidcrc"),
        ],
        cwd=workspace,
    )
    shutil.copyfile(
        support / "EPL-1.0.html",
        bin_path.parent / "licenses/rcomp-EPL-1.0.html",
    )


def install_tools(sdk: AppSdk, workspace: Path | None = None) -> AppSdk:
    """Exposes Python utilities/native modules and host command shims in SDK."""
    import shlex

    import pybind11_abseil.ok_status_singleton
    import pybind11_abseil.status

    import pybind11_abseil
    from symbian.native import require_native

    prefix = sdk.prefix
    native = require_native()
    licenses = Path(native.__file__).parent / "licenses"
    notices = (
        "A11-LICENSE",
        "Abseil-LICENSE",
        "nlohmann-json-LICENSE",
        "pybind11-LICENSE",
        "pybind11_abseil-LICENSE",
        "libusb-COPYING",
    )
    for name in notices:
        if not (licenses / name).is_file():
            raise StatusError(
                Code.FAILED_PRECONDITION,
                "Reinstall the SDK host package to include native notices: "
                + name,
            )
    tools = prefix / "lib/python"
    tools.mkdir(parents=True, exist_ok=True)
    shutil.copytree(
        Path(__file__).resolve().parents[1],
        tools / "symbian",
        ignore=shutil.ignore_patterns("tests", "__pycache__", "*.pyc"),
    )
    # The source tree keeps this GPL-compatible probe beside its native SDK
    # implementation; the wheel normally installs it as Python package data.
    resource_dir = tools / "symbian/sdk/resources"
    resource_dir.mkdir(parents=True, exist_ok=True)
    resource = files("symbian.sdk").joinpath("resources", "header_probe.cc")
    if resource.is_file():
        (resource_dir / "header_probe.cc").write_bytes(resource.read_bytes())
    else:
        source_root = workspace or Path(__file__).resolve().parents[2]
        shutil.copyfile(
            source_root / "cpp/symbian/sdk/probes/header_probe.cc",
            resource_dir / "header_probe.cc",
        )
    shutil.copytree(
        Path(pybind11_abseil.__file__).parent,
        tools / "pybind11_abseil",
        ignore=shutil.ignore_patterns("__pycache__", "*.pyc"),
    )
    for module, package in (
        (native, "symbian"),
        (pybind11_abseil.status, "pybind11_abseil"),
        (pybind11_abseil.ok_status_singleton, "pybind11_abseil"),
    ):
        original = Path(module.__file__)
        target = tools / package / original.name
        if not target.exists():
            shutil.copyfile(original, target)
    host_assets = Path(native.__file__).parent / "host"
    for name in ("libusb-1.0.a", "include/libusb.h"):
        source = host_assets / name
        if not source.is_file():
            raise StatusError(
                Code.FAILED_PRECONDITION,
                "Reinstall the SDK host package to include static libusb: "
                + name,
            )
        target = prefix / "lib/host" / name
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(source, target)
    shutil.copytree(licenses, prefix / "licenses/host", dirs_exist_ok=True)
    shutil.copytree(licenses, tools / "symbian/licenses", dirs_exist_ok=True)
    (prefix / "libexec").mkdir(exist_ok=True)
    (prefix / "libexec/python-bootstrap.py").write_bytes(
        files("symbian.project")
        .joinpath("templates", "python-bootstrap.py")
        .read_bytes()
    )
    bin_path = prefix / "bin"
    bin_path.mkdir()
    _install_resource_tools(bin_path, workspace=workspace)
    dependencies = {
        "clang++": sdk.compiler,
        "clang": sdk.c_compiler or sdk.compiler.parent / "clang",
        "ld.lld": sdk.linker,
        "python": sdk.python,
    }
    if sdk.ar and sdk.ranlib:
        dependencies["llvm-ar"] = sdk.ar
        dependencies["llvm-ranlib"] = sdk.ranlib
    if sdk.gdb:
        dependencies["arm-none-eabi-gdb"] = sdk.gdb
    for name, target in dependencies.items():
        preamble = ""
        if name == "python":
            preamble = (
                'sdk_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\n'
                'export PYTHONPATH="$sdk_dir/lib/python'
                '${PYTHONPATH:+:$PYTHONPATH}"\n'
            )
        wrapper = bin_path / name
        suffix = (
            ' "$sdk_dir/libexec/python-bootstrap.py"'
            if name == "python"
            else ""
        )
        wrapper.write_text(
            "#!/bin/sh\n"
            + preamble
            + "exec "
            + shlex.quote(str(target))
            + suffix
            + ' "$@"\n'
        )
        wrapper.chmod(0o755)
    wrapper = bin_path / "symbian"
    wrapper.write_text(
        "#!/bin/sh\n"
        'sdk_dir=$(CDPATH= cd -- "$(dirname -- "$0")/.." && pwd)\n'
        'export SYMBIAN_ACTIVE_SDK="$sdk_dir"\n'
        'export SYMBIAN_SDK_MANIFEST="$sdk_dir/sdk.json"\n'
        'exec "$sdk_dir/bin/python" -m symbian.cli "$@"\n'
    )
    wrapper.chmod(0o755)
    result = sdk.model_copy(
        update={
            "compiler": bin_path / "clang++",
            "c_compiler": bin_path / "clang",
            "linker": bin_path / "ld.lld",
            "ar": bin_path / "llvm-ar" if sdk.ar else None,
            "ranlib": bin_path / "llvm-ranlib" if sdk.ranlib else None,
            "python": bin_path / "python",
            "gdb": bin_path / "arm-none-eabi-gdb" if sdk.gdb else None,
        }
    )
    (prefix / "sdk.json").write_text(result.model_dump_json(indent=2) + "\n")
    return result


def install(
    destination: Path,
    workspace: Path | None = None,
    *,
    archive: Path | None = None,
) -> AppSdk:
    """Installs an observable SDK tree from source inputs or the active SDK.

    Args:
        destination: New directory for visible headers, libraries and tools.
        workspace: Prepared research checkout for a source installation.
        archive: Downloaded native SDK release archive.

    Returns:
        Installed and activated SDK manifest. Native release archives contain
        the compiler and build tools; emulator and firmware setup are separate.
    """
    destination = destination.resolve()
    if destination.exists():
        raise StatusError(
            Code.ALREADY_EXISTS, f"SDK already exists: {destination}"
        )
    if archive is not None:
        if workspace is not None:
            raise StatusError(
                Code.INVALID_ARGUMENT, "Select --archive or --workspace"
            )
        with tempfile.TemporaryDirectory(prefix="symbian-sdk-install-") as d:
            extracted = Path(d).resolve()
            try:
                with tarfile.open(archive) as bundle:
                    # Reject path escapes and external symlinks/hardlinks.
                    bundle.extractall(extracted, filter="data")
            except (OSError, tarfile.TarError) as error:
                raise StatusError(Code.INVALID_ARGUMENT, str(error)) from error
            original = AppSdk.load(extracted / "sdk.json")
            for name in (
                "prefix",
                "compiler",
                "c_compiler",
                "linker",
                "ar",
                "ranlib",
            ):
                value = getattr(original, name)
                if value is not None and not value.resolve().is_relative_to(
                    extracted
                ):
                    raise StatusError(
                        Code.INVALID_ARGUMENT,
                        f"Native archive has an external {name}: {value}",
                    )
            shutil.copytree(extracted, destination, symlinks=True)
        sdk = AppSdk.load(destination / "sdk.json")
        activate_sdk(sdk)
        return sdk
    if workspace:
        sdk = install_tools(
            prepare(workspace, destination), workspace=workspace
        )
    else:
        original = AppSdk.load(discover_sdk())
        if any(
            not (
                original.prefix
                / "lib"
                / architecture
                / "mbedtls/lib/cmake/MbedTLS/MbedTLSConfig.cmake"
            ).is_file()
            for architecture in original.architectures
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION,
                "Selected SDK predates the default Mbed TLS bundle; "
                "install with --workspace to export current source inputs",
            )
        if destination.is_relative_to(original.prefix):
            raise StatusError(
                Code.INVALID_ARGUMENT, "SDK destination overlaps source"
            )
        shutil.copytree(
            original.prefix,
            destination,
            symlinks=True,
            ignore=shutil.ignore_patterns(
                "__pycache__", "*.pyc", "digests.json", "host-dependencies.json"
            ),
        )
        if (
            not (destination / "bin/symbian").exists()
            and not (destination / "bin/symbian-native").exists()
        ):
            sdk = install_tools(
                original.model_copy(update={"prefix": destination})
            )
        else:

            def moved(path):
                return (
                    destination / path.relative_to(original.prefix)
                    if path and path.is_relative_to(original.prefix)
                    else path
                )

            sdk = original.model_copy(
                update={
                    "prefix": destination,
                    "compiler": moved(original.compiler),
                    "c_compiler": moved(original.c_compiler),
                    "linker": moved(original.linker),
                    "ar": moved(original.ar),
                    "ranlib": moved(original.ranlib),
                    "python": moved(original.python),
                    "gdb": moved(original.gdb),
                    "emulator": moved(original.emulator),
                    "firmware_importer": moved(original.firmware_importer),
                }
            )
            (destination / "sdk.json").write_text(
                sdk.model_dump_json(indent=2) + "\n"
            )
    activate_sdk(sdk)
    return AppSdk.load(destination / "sdk.json")
