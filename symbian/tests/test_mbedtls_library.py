"""Optional real Mbed TLS C archive and E32 DLL integration."""

import json
import os
import shutil
import subprocess
import tempfile
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
    not os.environ.get("SYMBIAN_MBEDTLS_SOURCE")
    or not os.environ.get("SYMBIAN_APP_SDK"),
    reason="Set SYMBIAN_MBEDTLS_SOURCE and SYMBIAN_APP_SDK",
)


@pytest.fixture(scope="module")
def artifacts(tmp_path_factory):
    """Builds the real C archive and SDK DLL once for format and guest tests."""
    tmp_path = tmp_path_factory.mktemp("mbedtls-dll")
    source = Path(os.environ["SYMBIAN_MBEDTLS_SOURCE"]).resolve()
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
        ["cmake", "--build", str(archive_build), "--target", "mbedcrypto"],
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


def test_mbedtls_sha256_subset_links_as_e32_dll(artifacts):
    """Validates the converted DLL, selected imports and consumer proxy."""
    _, _, dll_build, _ = artifacts
    image = inspect_image(dll_build / "mbedcrypto_probe.dll")
    assert image["dll"] and image["architecture"] == "armv6"
    assert len(image["exports"]) == 1
    assert image["exports"][0]["ordinal"] == 1
    assert not image["exports"][0]["absent"]
    assert [item["dll"] for item in image["imports"]] == ["euser.dll"]
    assert any(slot["ordinal"] == 609 for slot in image["imports"][0]["slots"])
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
def test_mbedtls_sha256_executes_through_dynamic_dll(
    artifacts, guest_client, tmp_path, backend, changed, reason
):
    """Runs real archive code through RLibrary and checks a changed input."""
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
