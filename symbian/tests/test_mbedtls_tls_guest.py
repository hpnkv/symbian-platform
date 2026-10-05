"""Opt-in authenticated TLS handshakes over the native guest TCP client."""

import json
import os
import shutil
import socket
import ssl
import subprocess
import tempfile
import threading
import time
from pathlib import Path

import msgpack
import pytest

from symbian import toolchain
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.firmware import EUSER_808, ROM_808
from symbian.emulator.launch import _digest, _stop
from symbian.project.sdk import AppSdk
from symbian.sdk import inspect_proxy

ROOT = Path(__file__).parents[2]
CERTIFICATES = ROOT / "third_party/mbedtls-symbian/tests/fixtures"

pytestmark = pytest.mark.skipif(
    not os.environ.get("SYMBIAN_RM807_TLS_HANDSHAKE"),
    reason="Set SYMBIAN_RM807_TLS_HANDSHAKE for the pinned emulator experiment",
)


@pytest.fixture(scope="module")
def guest_binaries(tmp_path_factory):
    """Build one opt-in DLL and three ordinary RLibrary consumers."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    build = tmp_path_factory.mktemp("tls-guest")
    archive = build / "archive"
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT / "third_party/mbedtls-symbian"),
            "-B",
            str(archive),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            "-DSYMBIAN_MBEDTLS_OPENSSL_COMPAT=OFF",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    subprocess.run(
        ["cmake", "--build", str(archive), "--target", "mbedtls"],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    dll_build = build / "dll"
    subprocess.run(
        [
            "cmake",
            "-S",
            str(ROOT / "probes/mbedtls_dll_probe"),
            "-B",
            str(dll_build),
            "-G",
            "Ninja",
            f"-DCMAKE_TOOLCHAIN_FILE={sdk.prefix}/cmake/symbian-arm.cmake",
            f"-DSYMBIAN_SDK_PREFIX={sdk.prefix}",
            f"-DMBEDTLS_SOURCE={ROOT}/third_party/mbedtls-symbian",
            f"-DMBEDTLS_ARCHIVE={archive}/libmbedcrypto.a",
            "-DSYMBIAN_RM807_TLS_HANDSHAKE_PROBE=ON",
        ],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    subprocess.run(
        ["cmake", "--build", str(dll_build)],
        check=True,
        stdout=subprocess.DEVNULL,
    )
    ordinals = {
        entry["symbol"]: entry["ordinal"]
        for entry in inspect_proxy(dll_build / "mbedcrypto_probe.dso")[
            "exports"
        ]
    }
    clients = {}
    for version in (12, 13):
        for mode in (0, 1, 2, 3):
            clients[(version, mode)] = build_client(
                sdk,
                build,
                version,
                mode,
                ordinal=ordinals["MbedNativeTlsHandshakeProbe"],
            )
        for mode in (0, 1, 2, 3):
            clients[("server", version, mode)] = build_client(
                sdk,
                build,
                version,
                mode,
                ordinal=ordinals["MbedNativeTlsServerProbe"],
                port=39098,
            )
            clients[("owned", version, mode)] = build_client(
                sdk,
                build,
                version,
                mode,
                ordinal=ordinals["MbedOwnedTlsServerProbe"],
                port=39098,
            )
    return dll_build / "mbedcrypto_probe.dll", clients


def build_client(sdk, build, version, mode, *, ordinal, port=39095):
    """Builds one normal E32 consumer of the DLL's TLS export."""
    project = build / f"client-{ordinal}-{version}-{mode}"
    shutil.copytree(
        ROOT / "probes/runtime_probe",
        project,
        ignore=shutil.ignore_patterns("cmake-build-*", "build", ".symbian"),
    )
    (project / "probe.cc").write_text(
        "#include <e32std.h>\n"
        '_LIT(KDll, "C:\\\\sys\\\\bin\\\\mbedcrypto_probe.dll");\n'
        "int main() {\n"
        "  RLibrary library;\n"
        "  TInt loaded = library.Load(KDll, KNullDesC);\n"
        "  if (loaded != KErrNone) return -210 + loaded;\n"
        "  using Probe = int (*)(int, int, int);\n"
        f"  auto probe = reinterpret_cast<Probe>(library.Lookup({ordinal}));\n"
        "  int result = probe == nullptr ? -211 : "
        f"probe({port}, {version}, {mode});\n"
        "  library.Close();\n"
        "  return result;\n"
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
    data = json.loads(presets.read_text())
    cache = data["configurePresets"][0]["cacheVariables"]
    cache["SYMBIAN_PLATFORM_ROOT"] = str(ROOT)
    cache["SYMBIAN_IMPORT_PROXIES"] = proxy
    presets.write_text(json.dumps(data))
    report = toolchain.build(
        project,
        build / f"build-{ordinal}-{version}-{mode}",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
    )
    return Path(report["artifact"])


@pytest.mark.parametrize("version", [12, 13])
@pytest.mark.parametrize("mode", [0, 1, 2, 3])
def test_authenticated_guest_tls(guest_binaries, tmp_path, version, mode):
    """Checks both protocols, trusted data, name mismatch, and unknown CA."""
    golden = ROOT / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned

    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(guest_binaries[0], guest_bin / "mbedcrypto_probe.dll")
    shutil.copyfile(
        guest_binaries[1][(version, mode)], guest_bin / "runtime_probe.exe"
    )
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    context = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    exact_version = (
        ssl.TLSVersion.TLSv1_2 if version == 12 else ssl.TLSVersion.TLSv1_3
    )
    context.minimum_version = exact_version
    context.maximum_version = exact_version
    certificate = "expired-cert.pem" if mode == 3 else "server-cert.pem"
    context.load_cert_chain(
        CERTIFICATES / certificate, CERTIFICATES / "server-key.pem"
    )
    results = []
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as listener:
        listener.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
        listener.bind(("127.0.0.1", 39095))
        listener.listen(1)

        def serve():
            try:
                listener.settimeout(30)
                connection, _ = listener.accept()
                with (
                    connection,
                    context.wrap_socket(connection, server_side=True) as tls,
                ):
                    tls.settimeout(30)
                    if mode == 0:
                        results.append(tls.recv(1))
                        tls.sendall(b"S")
            except (OSError, ssl.SSLError) as error:
                results.append(error)

        server = threading.Thread(target=serve)
        server.start()
        executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
        with tempfile.TemporaryDirectory(
            prefix="native-tls-", dir="/tmp"
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
                    assert process.wait(timeout=40) == 0
                    exits = control.exit_report()["process_exits"]
                    assert len(exits) == 1, exits
                    assert exits[0]["reason"] == 0, (
                        exits,
                        results,
                        (tmp_path / "frontend.log").read_text()[-4000:],
                    )
                finally:
                    _stop(process)
        server.join(timeout=2)
    if mode == 0:
        assert results == [b"H"]
    else:
        assert len(results) == 1 and isinstance(results[0], ssl.SSLError)
    assert {path: _digest(path) for path in pinned} == pinned


@pytest.mark.parametrize("version", [12, 13])
@pytest.mark.parametrize("mode", [0, 1, 2, 3])
@pytest.mark.parametrize("server_kind", ["server", "owned"])
def test_guest_mutual_tls_listener(
    guest_binaries, tmp_path, version, mode, server_kind
):
    """Checks raw and SDK-owned guest TLS listeners against the same peer."""
    golden = ROOT / ".symbian/instances/delight-import-01"
    pinned = {
        golden / "data/roms/rm-807/SYM.ROM": ROM_808,
        golden / "data/drives/z/rm-807/sys/bin/euser.dll": EUSER_808,
    }
    assert {path: _digest(path) for path in pinned} == pinned
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    guest_bin = instance / "data/drives/rm-807/c/sys/bin"
    guest_bin.mkdir(parents=True, exist_ok=True)
    shutil.copyfile(guest_binaries[0], guest_bin / "mbedcrypto_probe.dll")
    shutil.copyfile(
        guest_binaries[1][(server_kind, version, mode)],
        guest_bin / "runtime_probe.exe",
    )
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: true\n"
    )
    context = ssl.create_default_context(
        ssl.Purpose.SERVER_AUTH, cafile=str(CERTIFICATES / "server-cert.pem")
    )
    exact_version = (
        ssl.TLSVersion.TLSv1_2 if version == 12 else ssl.TLSVersion.TLSv1_3
    )
    context.minimum_version = exact_version
    context.maximum_version = exact_version
    if mode != 1:
        context.load_cert_chain(
            CERTIFICATES / "server-cert.pem",
            CERTIFICATES / "server-key.pem",
        )
    executable = ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1"
    with tempfile.TemporaryDirectory(
        prefix="native-mtls-", dir="/tmp"
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
                deadline = time.monotonic() + 25
                while True:
                    try:
                        connection = socket.create_connection(
                            ("127.0.0.1", 39098), timeout=1
                        )
                        break
                    except ConnectionRefusedError:
                        if time.monotonic() >= deadline:
                            raise
                        time.sleep(0.05)
                with connection:
                    try:
                        with context.wrap_socket(
                            connection, server_hostname="sdk-test"
                        ) as tls:
                            tls.settimeout(5)
                            if mode < 2:
                                tls.sendall(b"H")
                                reply = tls.recv(1)
                            elif mode == 3:
                                tls.sendall((4097).to_bytes(4, "big"))
                                reply = b""
                            else:
                                payload = msgpack.packb(
                                    {
                                        "v": 1,
                                        "id": 17,
                                        "kind": 2,
                                        "deadline_ms": 1500,
                                        "body": {},
                                        "future": 42,
                                    },
                                    use_bin_type=True,
                                )
                                tls.sendall(
                                    len(payload).to_bytes(4, "big") + payload
                                )

                                def receive_exact(count):
                                    output = bytearray()
                                    while len(output) < count:
                                        chunk = tls.recv(count - len(output))
                                        if not chunk:
                                            raise EOFError(
                                                "Guest closed a control frame"
                                            )
                                        output.extend(chunk)
                                    return bytes(output)

                                response_size = int.from_bytes(
                                    receive_exact(4), "big"
                                )
                                assert 0 < response_size <= 4096
                                response = msgpack.unpackb(
                                    receive_exact(response_size), raw=False
                                )
                                assert response["id"] == 17
                                assert response["kind"] == 4
                                assert response["future"] == 42
                                assert response["body"]["capabilities"] == [
                                    "status"
                                ]
                                reply = b"S"
                    except OSError:
                        reply = b""
                assert (reply == b"S") == (mode != 1 and mode != 3)
                assert process.wait(timeout=20) == 0
                exits = control.exit_report()["process_exits"]
                assert len(exits) == 1
                assert exits[0]["reason"] == 0, (
                    exits,
                    (tmp_path / "frontend.log").read_text()[-4000:],
                )
            finally:
                _stop(process)
    assert {path: _digest(path) for path in pinned} == pinned
