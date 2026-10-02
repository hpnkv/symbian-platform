"""Owned C++ DLL process-attach execution on preserved Belle firmware."""

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

WORKSPACE = os.environ.get("SYMBIAN_RUNTIME_WORKSPACE")
SDK_MANIFEST = os.environ.get("SYMBIAN_APP_SDK")
pytestmark = pytest.mark.skipif(
    not WORKSPACE or not SDK_MANIFEST,
    reason="Set SYMBIAN_RUNTIME_WORKSPACE and SYMBIAN_APP_SDK",
)


@pytest.fixture(scope="module")
def artifacts(tmp_path_factory):
    root = Path(WORKSPACE).resolve()
    sdk = AppSdk.load(Path(SDK_MANIFEST))
    output = tmp_path_factory.mktemp("C++ DLL lifecycle")
    dynamic_proxy = str(sdk.prefix / "proxies/euser/euser.dso")
    results = {}
    for architecture in ("armv5t", "armv6"):
        source = output / architecture / "dll-source"
        build = output / architecture / "dll-build"
        shutil.copytree(root / "examples/dll_lifecycle_probe", source)
        run(
            [
                "cmake",
                "-S",
                str(source),
                "-B",
                str(build),
                "-G",
                "Ninja",
                f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
                f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
                f"-DSYMBIAN_TARGET_ARCH={architecture}",
            ],
            cwd=source,
        )
        run(["cmake", "--build", str(build)], cwd=source)
        run(
            [
                "cmake",
                "--build",
                str(build),
                "--target",
                "lifecycle_probe_proxy",
            ],
            cwd=source,
        )
        positive = output / architecture / "constructed.dll"
        shutil.copyfile(build / "lifecycle_probe.dll", positive)
        image = inspect_image(positive)
        assert image["dll"] and image["architecture"] == architecture
        assert image["bss_size"] > 0 and image["code_relocations"]

        client = output / architecture / "client"
        shutil.copytree(root / "examples/runtime_probe", client)
        (client / "probe.cc").write_text(
            'extern "C" int SymbianLifecycleState();\n'
            'extern "C" int RuntimeMain() {\n'
            "  return SymbianLifecycleState() == 12 ? 0 : -122;\n"
            "}\n"
        )
        manifest = client / "symbian.toml"
        manifest.write_text(
            manifest.read_text().replace(
                'import_proxies = ["../../.symbian/runtime-sdk/'
                'euser/euser.dso"]',
                "import_proxies = "
                + json.dumps(
                    [
                        str(root / ".symbian/runtime-sdk/euser/euser.dso"),
                        str(
                            build / "lifecycle_probe-import/lifecycle_probe.dso"
                        ),
                    ]
                ),
            )
        )
        presets = json.loads((client / "CMakePresets.json").read_text())
        variables = presets["configurePresets"][0]["cacheVariables"]
        variables["SYMBIAN_PLATFORM_ROOT"] = str(root)
        variables["SYMBIAN_IMPORT_PROXIES"] = ";".join(
            (
                str(root / ".symbian/runtime-sdk/euser/euser.dso"),
                str(build / "lifecycle_probe-import/lifecycle_probe.dso"),
            )
        )
        (client / "CMakePresets.json").write_text(json.dumps(presets))
        app = toolchain.build(
            client,
            output / architecture / "client-build",
            str(sdk.compiler),
            str(sdk.linker),
            architecture=architecture,
        )
        assert app["reproducible"]
        assert {item["dll"] for item in app["e32"]["imports"]} == {
            "euser.dll",
            "lifecycle_probe.dll",
        }

        dynamic_client = output / architecture / "dynamic-client"
        shutil.copytree(root / "examples/runtime_probe", dynamic_client)
        (dynamic_client / "probe.cc").write_text(
            "#include <e32std.h>\n"
            '_LIT(KLifecycleName, "C:\\\\sys\\\\bin\\\\lifecycle_probe.dll");\n'
            'extern "C" int RuntimeMain() {\n'
            "  RLibrary library;\n"
            "  const TInt loaded = library.Load(KLifecycleName, KNullDesC);\n"
            "  if (loaded != KErrNone) return -120 + loaded;\n"
            "  auto state = reinterpret_cast<TInt (*)()>(library.Lookup(1));\n"
            "  auto sink = reinterpret_cast<void (*)(volatile int*)>(\n"
            "      library.Lookup(2));\n"
            "  if (state == nullptr || sink == nullptr) {\n"
            "    library.Close();\n"
            "    return -123;\n"
            "  }\n"
            "  if (state() != 12) {\n"
            "    library.Close();\n"
            "    return -124;\n"
            "  }\n"
            "  volatile int observed = 0;\n"
            "  sink(&observed);\n"
            "  library.Close();\n"
            "  return observed == 34 ? 0 : -125;\n"
            "}\n"
        )
        dynamic_manifest = dynamic_client / "symbian.toml"
        dynamic_manifest.write_text(
            dynamic_manifest.read_text().replace(
                "../../.symbian/runtime-sdk/euser/euser.dso",
                dynamic_proxy,
            )
        )
        dynamic_presets = json.loads(
            (dynamic_client / "CMakePresets.json").read_text()
        )
        dynamic_variables = dynamic_presets["configurePresets"][0][
            "cacheVariables"
        ]
        dynamic_variables["SYMBIAN_PLATFORM_ROOT"] = str(root)
        dynamic_variables["SYMBIAN_IMPORT_PROXIES"] = dynamic_proxy
        (dynamic_client / "CMakePresets.json").write_text(
            json.dumps(dynamic_presets)
        )
        dynamic_app = toolchain.build(
            dynamic_client,
            output / architecture / "dynamic-build",
            str(sdk.compiler),
            str(sdk.linker),
            architecture=architecture,
        )
        assert dynamic_app["reproducible"]
        assert {item["dll"] for item in dynamic_app["e32"]["imports"]} == {
            "euser.dll"
        }

        probe = source / "probe.cc"
        probe.write_text(probe.read_text().replace("= 12;", "= 13;"))
        run(["cmake", "--build", str(build)], cwd=source)
        negative = output / architecture / "changed-constructor.dll"
        shutil.copyfile(build / "lifecycle_probe.dll", negative)
        assert _digest(positive) != _digest(negative)
        results[architecture] = (app, positive, negative, dynamic_app)
    return results


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
@pytest.mark.parametrize("changed,reason", [(False, 0), (True, -122)])
def test_dll_constructor_runs_before_client_and_change_is_detected(
    tmp_path, artifacts, architecture, backend, changed, reason
):
    app, positive, negative, _ = artifacts[architecture]
    _run_client(
        tmp_path,
        app,
        negative if changed else positive,
        backend,
        reason,
    )


@pytest.mark.parametrize("architecture", ["armv5t", "armv6"])
@pytest.mark.parametrize("backend", ["dynarmic", "dyncom"])
@pytest.mark.parametrize("present,reason", [(True, 0), (False, -121)])
def test_dynamic_library_load_lookup_detach_and_missing_file(
    tmp_path, artifacts, architecture, backend, present, reason
):
    _, positive, _, dynamic_app = artifacts[architecture]
    _run_client(
        tmp_path, dynamic_app, positive if present else None, backend, reason
    )


def _run_client(tmp_path, app, dll, backend, reason):
    root = Path(WORKSPACE).resolve()
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
    shutil.copyfile(app["artifact"], guest_bin / "runtime_probe.exe")
    if dll is not None:
        shutil.copyfile(dll, guest_bin / "lifecycle_probe.dll")
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    executable = root / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="dll-lifecycle-", dir="/tmp"
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
