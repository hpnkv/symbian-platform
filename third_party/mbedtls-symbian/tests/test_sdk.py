"""Opt-in SDK archive consumption, image publication and guest execution."""

import hashlib
import json
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

import pytest

ROOT = Path(__file__).resolve().parents[1]
SDK = os.environ.get("SYMBIAN_SDK_PREFIX")
WORKSPACE = os.environ.get("SYMBIAN_MBEDTLS_WORKSPACE")
pytestmark = pytest.mark.skipif(
    not SDK, reason="Set SYMBIAN_SDK_PREFIX to a current materialized SDK"
)


def digest(path):
    return hashlib.sha256(path.read_bytes()).hexdigest()


def command(arguments, log):
    with log.open("w") as output:
        result = subprocess.run(
            list(map(str, arguments)),
            stdout=output,
            stderr=subprocess.STDOUT,
            timeout=120,
            check=False,
        )
    assert result.returncode == 0, log.read_text()[-6000:]


@pytest.fixture(scope="module")
def artifacts(tmp_path_factory):
    from symbian.e32 import convert_imported_executable, inspect_image

    output = tmp_path_factory.mktemp("sdk-artifacts")
    sdk = Path(SDK).resolve()
    before = {
        name: digest(sdk / name)
        for name in (
            "sdk.json",
            "digests.json",
            "cmake/symbian-arm.cmake",
            "lib/armv6/libsymbian_guest_runtime.a",
            "lib/armv5t/libsymbian_guest_runtime.a",
        )
    }
    result = {}
    for architecture in ("armv6", "armv5t"):
        for changed in (False, True):
            build = output / f"{architecture}-{int(changed)}"
            command(
                [
                    "cmake",
                    "-S",
                    ROOT,
                    "-B",
                    build,
                    "-G",
                    "Ninja",
                    f"-DSYMBIAN_SDK_PREFIX={sdk}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    "-DCMAKE_BUILD_TYPE=Debug",
                    "-DSYMBIAN_MBEDTLS_GUEST_PROBE=ON",
                    f"-DSYMBIAN_MBEDTLS_CHANGED_VECTOR={'ON' if changed else 'OFF'}",
                ],
                output / f"{build.name}-configure.log",
            )
            command(
                ["cmake", "--build", build, "--parallel", "6"],
                output / f"{build.name}-build.log",
            )
            elf = build / "crypto_probe.elf"
            image = convert_imported_executable(
                elf.read_bytes(),
                [(sdk / "proxies/euser/euser.dso").read_bytes()],
                0xE0000821,
            )
            exe = elf.with_suffix(".exe")
            exe.write_bytes(image)
            info = inspect_image(exe)
            assert info["architecture"] == architecture
            assert info["dll"] is False
            result[architecture, changed] = exe
            (build / "image.json").write_text(json.dumps(info, indent=2))
            command(
                ["/opt/homebrew/opt/llvm/bin/llvm-dwarfdump", "--verify", elf],
                output / f"{build.name}-dwarf.log",
            )
    assert before == {name: digest(sdk / name) for name in before}
    (output / "sdk-inputs.json").write_text(json.dumps(before, indent=2))
    return result


@pytest.mark.parametrize("architecture", ["armv6", "armv5t"])
def test_installed_package_consumption(tmp_path, artifacts, architecture):
    # Consume an installed copy after moving its prefix. No build-tree paths or
    # selected-SDK paths should leak into the exported MbedTLS targets.
    sdk = Path(SDK).resolve()
    build = artifacts[architecture, False].parent
    prefix = tmp_path / "installed"
    command(
        ["cmake", "--install", build, "--prefix", prefix],
        tmp_path / "install.log",
    )
    moved = tmp_path / "relocated"
    prefix.rename(moved)
    source = tmp_path / "consumer"
    source.mkdir()
    (source / "main.cc").write_text(
        '#include "mbedtls/sha256.h"\n'
        'extern "C" int GuiMain() { unsigned char output[32]; '
        'return mbedtls_sha256((const unsigned char*)"abc", 3, output, 0); }\n'
    )
    for name in ("startup.cc", "startup.S", "image.ld"):
        shutil.copyfile(ROOT / "examples/crypto_probe" / name, source / name)
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(consumer LANGUAGES CXX ASM)\n"
        'list(PREPEND CMAKE_MODULE_PATH "${SYMBIAN_SDK_PREFIX}/cmake")\n'
        "find_package(MbedTLS 3.4.1 EXACT CONFIG REQUIRED)\n"
        'set(SYMBIAN_IMPORT_PROXIES "${SYMBIAN_SDK_PREFIX}/proxies/euser/euser.dso")\n'
        "symbian_add_import_executable(consumer STARTUP startup.S "
        "LINKER_SCRIPT image.ld SOURCES startup.cc main.cc)\n"
        "target_link_libraries(consumer PRIVATE MbedTLS::mbedtls)\n"
        "target_link_options(consumer PRIVATE --gc-sections)\n"
    )
    command(
        [
            "cmake",
            "-S",
            source,
            "-B",
            tmp_path / "build",
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={ROOT}/cmake/SymbianMbedTLS.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk}",
            f"-DSYMBIAN_TARGET_ARCH={architecture}",
            f"-DMbedTLS_DIR={moved}/lib/cmake/MbedTLS",
        ],
        tmp_path / "consumer-configure.log",
    )
    command(
        ["cmake", "--build", tmp_path / "build"],
        tmp_path / "consumer-build.log",
    )
    from symbian.e32 import convert_imported_executable

    elf = tmp_path / "build/consumer.elf"
    image = convert_imported_executable(
        elf.read_bytes(),
        [(sdk / "proxies/euser/euser.dso").read_bytes()],
        0xE0000821,
    )
    elf.with_suffix(".exe").write_bytes(image)


@pytest.mark.skipif(
    not WORKSPACE, reason="Set SYMBIAN_MBEDTLS_WORKSPACE for guest execution"
)
@pytest.mark.parametrize("architecture", ["armv6", "armv5t"])
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
@pytest.mark.parametrize("changed,reason", [(False, 0), (True, -121)])
def test_crypto_executes_in_guest(
    tmp_path, artifacts, architecture, backend, changed, reason
):
    from symbian.emulator import Control
    from symbian.emulator.background import executable_for_session
    from symbian.emulator.firmware import EUSER_808, ROM_808
    from symbian.emulator.launch import _stop

    workspace = Path(WORKSPACE).resolve()
    golden = workspace / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: digest(path) for path in pinned} == pinned
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    target = instance / "data/drives/rm-807/c/sys/bin/crypto_probe.exe"
    target.parent.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(artifacts[architecture, changed], target)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = workspace / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="tls-probe-", dir="/tmp"
    ) as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\crypto_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(
                    json.dumps(exits, indent=2)
                )
                assert exits == [
                    {
                        "uid": 0xE0000821,
                        "name": "crypto_probe[e0000821]0001",
                        "type": 0,
                        "reason": reason,
                    }
                ]
            finally:
                _stop(process)
                assert {path: digest(path) for path in pinned} == pinned
