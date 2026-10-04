"""Optional real Mbed TLS C archive and E32 DLL integration."""

import json
import os
import shutil
import socket
import subprocess
import tempfile
import threading
import time
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.e32 import inspect_image
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import EUSER_808, ROM_808
from symbian.emulator.launch import _digest, _stop
from symbian.process import run
from symbian.project.sdk import AppSdk
from symbian.sdk import inspect_proxy

pytestmark = pytest.mark.skipif(
    not os.environ.get("SYMBIAN_APP_SDK"),
    reason="Set SYMBIAN_APP_SDK",
)


@pytest.fixture(scope="module")
def artifacts(tmp_path_factory):
    """Builds the real C archive and SDK DLL once for format and guest tests."""
    tmp_path = tmp_path_factory.mktemp("mbedtls-dll")
    source = Path(__file__).parents[2] / "third_party/mbedtls-symbian"
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_APP_SDK"]))
    archive_build = tmp_path / "mbedtls archive"
    run(
        [
            "cmake",
            "-S",
            str(source),
            "-B",
            str(archive_build),
            "-G",
            "Ninja",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            "-DSYMBIAN_MBEDTLS_GUEST_PROBE=OFF",
        ],
        cwd=source,
    )
    run(
        ["cmake", "--build", str(archive_build), "--target", "mbedtls"],
        cwd=source,
    )
    archive = archive_build / "libmbedcrypto.a"
    assert archive.is_file()
    example = Path(__file__).parents[2] / "examples/mbedtls_dll_probe"
    dll_build = tmp_path / "mbedtls DLL"
    run(
        [
            "cmake",
            "-S",
            str(example),
            "-B",
            str(dll_build),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            f"-DMBEDTLS_SOURCE={source}",
            f"-DMBEDTLS_ARCHIVE={archive}",
        ],
        cwd=example,
    )
    run(["cmake", "--build", str(dll_build)], cwd=example)
    run(
        [
            "cmake",
            "--build",
            str(dll_build),
            "--target",
            "mbedcrypto_probe_proxy",
        ],
        cwd=example,
    )
    return source, sdk, dll_build, tmp_path


def test_mbedtls_crypto_x509_links_as_e32_dll(artifacts):
    """Validates the crypto/X.509 DLL, imports and consumer proxy."""
    _, _, dll_build, _ = artifacts
    image = inspect_image(dll_build / "mbedcrypto_probe.dll")
    assert image["dll"] and image["architecture"] == "armv6"
    assert [entry["ordinal"] for entry in image["exports"]] == [1, 2, 3, 4, 5]
    assert all(not entry["absent"] for entry in image["exports"])
    assert [item["dll"] for item in image["imports"]] == [
        "euser.dll",
        "libc.dll",
        "libpthread.dll",
    ]
    assert any(slot["ordinal"] == 641 for slot in image["imports"][0]["slots"])
    proxy = inspect_proxy(
        dll_build / "mbedcrypto_probe-import/mbedcrypto_probe.dso"
    )
    assert proxy["target_dll"] == "mbedcrypto_probe.dll"
    assert (dll_build / "mbedcrypto_probe_elf.elf").is_file()


@pytest.fixture(scope="module")
def guest_client(artifacts):
    """Builds a dynamic RLibrary consumer using the real EUSER ordinals."""
    _, sdk, _, output = artifacts
    root = Path(__file__).parents[2]
    proxy = str(sdk.prefix / "proxies/euser/euser.dso")
    result = {}
    for changed in (False, True):
        project = output / ("changed-client" if changed else "client")
        shutil.copytree(root / "examples/runtime_probe", project)
        source = (
            "#include <e32std.h>\n"
            "#include <stddef.h>\n"
            '_LIT(KShaName, "C:\\\\sys\\\\bin\\\\mbedcrypto_probe.dll");\n'
            'extern "C" int RuntimeMain() {\n'
            "  RLibrary library;\n"
            "  TInt loaded = library.Load(KShaName, KNullDesC);\n"
            "  if (loaded != KErrNone) return -130 + loaded;\n"
            "  using Sha = int (*)(const unsigned char*, size_t, "
            "unsigned char*);\n"
            "  auto sha = reinterpret_cast<Sha>(library.Lookup(1));\n"
            "  if (sha == nullptr) { library.Close(); return -131; }\n"
            "  const unsigned char input[] = {'a', 'b', "
            + ("'d'" if changed else "'c'")
            + "};\n"
            "  unsigned char digest[32] = {};\n"
            "  TInt result = sha(input, sizeof(input), digest);\n"
            "  const unsigned char expected[] = {\n"
            "      0xba, 0x78, 0x16, 0xbf, 0x8f, 0x01, 0xcf, 0xea,\n"
            "      0x41, 0x41, 0x40, 0xde, 0x5d, 0xae, 0x22, 0x23,\n"
            "      0xb0, 0x03, 0x61, 0xa3, 0x96, 0x17, 0x7a, 0x9c,\n"
            "      0xb4, 0x10, 0xff, 0x61, 0xf2, 0x00, 0x15, 0xad};\n"
            "  if (result == 0) {\n"
            "    for (int i = 0; i < 32; ++i) {\n"
            "      if (digest[i] != expected[i]) result = -132;\n"
            "    }\n"
            "  }\n"
            "  if (result == 0) {\n"
            "    using UtcProbe = int (*)();\n"
            "    auto utc = reinterpret_cast<UtcProbe>(library.Lookup(2));\n"
            "    result = utc == nullptr ? -135 : utc();\n"
            "  }\n"
            "  if (result == 0) {\n"
            "    using Verify = int (*)();\n"
            "    auto verify = reinterpret_cast<Verify>(library.Lookup(3));\n"
            "    result = verify == nullptr ? -138 : verify();\n"
            "  }\n"
            "  if (result == 0) {\n"
            "    using SocketProbe = int (*)();\n"
            "    auto socket_probe = "
            "reinterpret_cast<SocketProbe>(library.Lookup(4));\n"
            "    result = socket_probe == nullptr ? -142 : socket_probe();\n"
            "  }\n"
            "  if (result == 0) {\n"
            "    using EntropyProbe = int (*)();\n"
            "    auto entropy_probe = "
            "reinterpret_cast<EntropyProbe>(library.Lookup(5));\n"
            "    result = entropy_probe == nullptr ? -144 : entropy_probe();\n"
            "  }\n"
            "  library.Close();\n"
            "  return result;\n"
            "}\n"
        )
        (project / "probe.cc").write_text(source)
        manifest = project / "symbian.toml"
        manifest.write_text(
            manifest.read_text().replace(
                "../../.symbian/runtime-sdk/euser/euser.dso", proxy
            )
        )
        presets = project / "CMakePresets.json"
        data = json.loads(presets.read_text())
        variables = data["configurePresets"][0]["cacheVariables"]
        variables["SYMBIAN_PLATFORM_ROOT"] = str(root)
        variables["SYMBIAN_IMPORT_PROXIES"] = proxy
        presets.write_text(json.dumps(data))
        result[changed] = toolchain.build(
            project,
            output / ("changed-build" if changed else "client-build"),
            str(sdk.compiler),
            str(sdk.linker),
            architecture="armv6",
        )
    return result


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_RUNTIME_WORKSPACE"),
    reason="Set SYMBIAN_RUNTIME_WORKSPACE for named-firmware execution",
)
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
@pytest.mark.parametrize("changed,reason", [(False, 0), (True, -132)])
def test_mbedtls_crypto_x509_executes_through_dynamic_dll(
    artifacts, guest_client, tmp_path, backend, changed, reason
):
    """Checks guest crypto, UTC, certificates, and callbacks via RLibrary."""
    root = Path(os.environ["SYMBIAN_RUNTIME_WORKSPACE"]).resolve()
    golden = root / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        guest_client[changed]["artifact"], guest_bin / "runtime_probe.exe"
    )
    shutil.copyfile(
        artifacts[2] / "mbedcrypto_probe.dll",
        guest_bin / "mbedcrypto_probe.dll",
    )
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="mbedtls-dll-", dir="/tmp"
    ) as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\runtime_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                assert exits == [
                    {
                        "uid": 0xE0000813,
                        "name": "runtime_probe[e0000813]0001",
                        "type": 0,
                        "reason": reason,
                    }
                ]
            finally:
                _stop(process)
                assert {path: _digest(path) for path in pinned} == pinned


@pytest.fixture(scope="module")
def rm807_entropy_dll(artifacts):
    """Builds the opt-in ROM-specific entropy adapter, never the default SDK."""
    source, sdk, _, output = artifacts
    example = Path(__file__).parents[2] / "examples/mbedtls_dll_probe"
    dll_build = output / "rm807 entropy DLL"
    archive = output / "mbedtls archive" / "libmbedcrypto.a"
    run(
        [
            "cmake",
            "-S",
            str(example),
            "-B",
            str(dll_build),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            f"-DMBEDTLS_SOURCE={source}",
            f"-DMBEDTLS_ARCHIVE={archive}",
            "-DSYMBIAN_RM807_ENTROPY_PROBE=ON",
        ],
        cwd=example,
    )
    run(["cmake", "--build", str(dll_build)], cwd=example)
    return dll_build / "mbedcrypto_probe.dll"


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_RM807_ENTROPY"),
    reason="Set SYMBIAN_RM807_ENTROPY for the patched named-ROM experiment",
)
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
def test_rm807_secure_entropy_executes_in_emulator(
    rm807_entropy_dll, guest_client, tmp_path, backend
):
    """Checks a ROM-specific SVC route with a patched disposable emulator."""
    root = Path(os.environ["SYMBIAN_RUNTIME_WORKSPACE"]).resolve()
    golden = root / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(
        guest_client[False]["artifact"], guest_bin / "runtime_probe.exe"
    )
    shutil.copyfile(rm807_entropy_dll, guest_bin / "mbedcrypto_probe.dll")
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="rm807-entropy-", dir="/tmp"
    ) as private:
        control = Control(Path(private) / "control.sock")
        env = dict(os.environ)
        env.update(
            EKA2L1_DATA_ROOT=str(instance),
            EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
            EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
            **background_environment(),
        )
        with (tmp_path / "frontend.log").open("w") as log:
            process = subprocess.Popen(
                [
                    executable_for_session(executable, Path(private)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\runtime_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=30) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                assert exits == [
                    {
                        "uid": 0xE0000813,
                        "name": "runtime_probe[e0000813]0001",
                        "type": 0,
                        "reason": 0,
                    }
                ]
            finally:
                _stop(process)
                assert {path: _digest(path) for path in pinned} == pinned


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_RM807_SOCKET"),
    reason="Set SYMBIAN_RM807_SOCKET for the patched socket experiment",
)
def test_rm807_nonblocking_receive_and_cancel_in_emulator(artifacts, tmp_path):
    """Checks connected WANT_READ, delayed delivery and cancellation."""
    source, sdk, _, _ = artifacts
    root = Path(__file__).parents[2]
    archive = artifacts[3] / "mbedtls archive" / "libmbedcrypto.a"
    example = root / "examples/mbedtls_dll_probe"
    dll_build = tmp_path / "socket DLL"
    run(
        [
            "cmake",
            "-S",
            str(example),
            "-B",
            str(dll_build),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            f"-DMBEDTLS_SOURCE={source}",
            f"-DMBEDTLS_ARCHIVE={archive}",
            "-DSYMBIAN_RM807_SOCKET_PROBE=ON",
        ],
        cwd=example,
    )
    run(["cmake", "--build", str(dll_build)], cwd=example)
    project = tmp_path / "client"
    shutil.copytree(root / "examples/runtime_probe", project)
    (project / "probe.cc").write_text(
        "#include <e32std.h>\n"
        '_LIT(KName, "C:\\\\sys\\\\bin\\\\mbedcrypto_probe.dll");\n'
        'extern "C" int RuntimeMain() {\n'
        " RLibrary library;\n"
        " TInt loaded = library.Load(KName, KNullDesC);\n"
        " if (loaded != KErrNone) return -160 + loaded;\n"
        " using Probe = int (*)(int);\n"
        " auto probe = reinterpret_cast<Probe>(library.Lookup(6));\n"
        " TInt result = probe == nullptr ? -161 : probe(39093);\n"
        " library.Close();\n"
        " return result;\n"
        "}\n"
    )
    proxy = str(sdk.prefix / "proxies/euser/euser.dso")
    manifest = project / "symbian.toml"
    manifest.write_text(
        manifest.read_text().replace(
            "../../.symbian/runtime-sdk/euser/euser.dso", proxy
        )
    )
    presets = project / "CMakePresets.json"
    config = json.loads(presets.read_text())
    variables = config["configurePresets"][0]["cacheVariables"]
    variables["SYMBIAN_PLATFORM_ROOT"] = str(root)
    variables["SYMBIAN_IMPORT_PROXIES"] = proxy
    presets.write_text(json.dumps(config))
    artifact = toolchain.build(
        project,
        tmp_path / "client-build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
    )["artifact"]
    golden = root / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(artifact, guest_bin / "runtime_probe.exe")
    shutil.copyfile(
        dll_build / "mbedcrypto_probe.dll", guest_bin / "mbedcrypto_probe.dll"
    )
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    results = []
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 39093))
        listener.listen(1)

        def deliver():
            try:
                listener.settimeout(25)
                connection, _ = listener.accept()
                with connection:
                    time.sleep(0.2)
                    connection.sendall(b"R")
                    results.append("delivered")
            except OSError as error:
                results.append(str(error))

        sender = threading.Thread(target=deliver)
        sender.start()
        executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
        with tempfile.TemporaryDirectory(
            prefix="rm807-socket-", dir="/tmp"
        ) as private:
            control = Control(Path(private) / "control.sock")
            env = dict(os.environ)
            env.update(
                EKA2L1_DATA_ROOT=str(instance),
                EKA2L1_EXPERIMENTAL_SVC_PROFILE="rm807-113.010.1508",
                EKA2L1_RESEARCH_CONTROL_SOCKET=str(control.endpoint),
                **background_environment(),
            )
            with (tmp_path / "frontend.log").open("w") as log:
                process = subprocess.Popen(
                    [
                        executable_for_session(executable, Path(private)),
                        "--device",
                        "RM-807",
                        "--run",
                        "C:\\sys\\bin\\runtime_probe.exe",
                    ],
                    env=env,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                )
                try:
                    assert process.wait(timeout=30) == 0
                    assert (
                        control.exit_report()["process_exits"][0]["reason"] == 0
                    )
                finally:
                    _stop(process)
        sender.join(timeout=2)
    assert results == ["delivered"]
    assert {path: _digest(path) for path in pinned} == pinned
