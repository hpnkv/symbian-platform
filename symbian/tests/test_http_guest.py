"""Opt-in native HTTP acceptance against live Internet sites in the emulator."""

import hashlib
import json
import os
import shutil
import socket
import ssl
import subprocess
import tempfile
from pathlib import Path

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

ROOT = Path(__file__).parents[2]
GOLDEN = Path(
    os.environ.get(
        "SYMBIAN_HTTP_GOLDEN_ROOT",
        ROOT / ".symbian/instances/delight-import-01",
    )
)
EMULATOR = Path(
    os.environ.get(
        "SYMBIAN_EKA2L1_EXECUTABLE",
        ROOT / "build/eka2l1/bin/EKA2L1.app/Contents/MacOS/EKA2L1",
    )
)
CASES = (
    ("example.com", 80, 0, False, False),
    ("example.com", 443, 12, False, False),
    ("example.com", 443, 13, False, False),
    ("www.cloudflare.com", 443, 12, True, False),
    ("www.cloudflare.com", 443, 13, True, False),
    ("example.com", 443, 12, False, True),
    ("example.com", 443, 13, False, True),
)
pytestmark = pytest.mark.skipif(
    not os.environ.get("SYMBIAN_HTTP_LIVE_GUEST"),
    reason="Set SYMBIAN_HTTP_LIVE_GUEST for live Internet emulator acceptance",
)


def baseline_digest(root):
    """Records all files in the preserved ROM/Z baseline."""
    files = sorted(path for path in root.rglob("*") if path.is_file())
    return {
        str(path.relative_to(root)): _digest(path)
        for path in files
        if "/drives/z/" in str(path) or "/roms/" in str(path)
    }


@pytest.fixture(scope="module")
def http_image(tmp_path_factory):
    """Builds current native code with explicit roots and addresses."""
    sdk = AppSdk.load(Path(os.environ["SYMBIAN_SDK_MANIFEST"]))
    output = tmp_path_factory.mktemp("http-guest-build")
    project = output / "project"
    shutil.copytree(ROOT / "probes/http_probe", project)
    roots = []
    context = ssl.create_default_context()
    for info, der in zip(
        context.get_ca_certs(),
        context.get_ca_certs(binary_form=True),
        strict=True,
    ):
        names = [
            value
            for rdn in info["subject"]
            for name, value in rdn
            if name == "commonName"
        ]
        if names and names[0] in (
            "GTS Root R1",
            "GTS Root R4",
            "SSL.com TLS ECC Root CA 2022",
        ):
            roots.append(ssl.DER_cert_to_PEM_cert(der).encode())
    assert len(roots) == 3, "The selected public trust anchors are required"
    bundle = b"".join(roots)
    addresses = {host: socket.gethostbyname(host) for host, *_ in CASES}
    config = (
        (project / "probe_config.h")
        .read_text()
        .split("inline constexpr ProbeCase kCases[]")[0]
    )
    config += "inline constexpr ProbeCase kCases[] = {\n"
    for host, port, version, http2, reject in CASES:
        address = ", ".join(addresses[host].split("."))
        config += (
            f'  {{"{host}", {{{address}}}, {port}, {version}, '
            f"{str(http2).lower()}, {str(reject).lower()}}},\n"
        )
    config += '};\ninline constexpr char kRoots[] = R"pem(\n'
    config += bundle.decode() + ')pem";\n'
    for name, filename in (
        ("kServerCertificate", "server-cert.pem"),
        ("kServerKey", "server-key.pem"),
    ):
        pem = (
            ROOT / "third_party/mbedtls-symbian/tests/fixtures" / filename
        ).read_text()
        config += f'inline constexpr char {name}[] = R"pem({pem})pem";\n'
    (project / "probe_config.h").write_text(config)
    presets = project / "CMakePresets.json"
    data = json.loads(presets.read_text())
    data["configurePresets"][0]["toolchainFile"] = str(
        sdk.prefix / "cmake/symbian-arm.cmake"
    )
    data["configurePresets"][0]["cacheVariables"]["SYMBIAN_SDK_PREFIX"] = str(
        sdk.prefix
    )
    presets.write_text(json.dumps(data))
    report = toolchain.build(
        project,
        output / "build",
        str(sdk.compiler),
        str(sdk.linker),
        architecture="armv6",
        cmake_variables={
            **(
                {}
                if os.environ.get("SYMBIAN_HTTP_EXPORTED_SDK")
                else {"SYMBIAN_HTTP_WORKSPACE_SOURCE": str(ROOT)}
            ),
            "SYMBIAN_HTTP_ENTROPY_SOURCE": str(
                ROOT / "agent_service/sdk_entropy_rm807.cc"
            ),
            "SYMBIAN_HTTP_ENTROPY_ASSEMBLY": str(
                ROOT / "agent_service/sdk_entropy_rm807.S"
            ),
        },
    )
    (output / "inputs.json").write_text(
        json.dumps(
            {
                "addresses": addresses,
                "ca_sha256": hashlib.sha256(bundle).hexdigest(),
                "image_sha256": _digest(Path(report["artifact"])),
                "sdk": str(sdk.prefix),
            },
            indent=2,
        )
    )
    return Path(report["artifact"])


@pytest.mark.parametrize("case", range(len(CASES)))
def test_live_native_http_client(http_image, tmp_path, case):
    """Checks guest sockets, authenticated TLS and streamed HTTP bodies."""
    golden = GOLDEN
    assert _digest(golden / "data/roms/rm-807/SYM.ROM") == ROM_808
    assert (
        _digest(golden / "data/drives/z/rm-807/sys/bin/euser.dll") == EUSER_808
    )
    before = baseline_digest(golden)
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    drive = instance / "data/drives/rm-807/c"
    (drive / "sys/bin").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(http_image, drive / "sys/bin/http_probe.exe")
    (drive / "http-case.txt").write_text(str(case))
    (drive / "http-result.txt").write_text("")
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: false\n"
    )
    executable = EMULATOR
    with tempfile.TemporaryDirectory(prefix="native-http-", dir="/tmp") as p:
        control = Control(Path(p) / "control.sock")
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
                    executable_for_session(executable, Path(p)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\http_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                assert process.wait(timeout=55) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                result = (drive / "http-result.txt").read_text()
                assert len(exits) == 1 and exits[0]["reason"] == 0, (
                    exits,
                    result,
                    (tmp_path / "frontend.log").read_text()[-4000:],
                )
            finally:
                _stop(process)
    assert baseline_digest(golden) == before
    after = baseline_digest(instance)
    changed = [
        name
        for name in before.keys() & after.keys()
        if before[name] != after[name]
    ]
    missing = sorted(before.keys() - after.keys())
    added = sorted(after.keys() - before.keys())
    (tmp_path / "disposable-z-changes.json").write_text(
        json.dumps({"changed": changed, "missing": missing, "added": added})
    )
    replacements = {
        "data/drives/z/rm-807/sys/bin/avkonfep.dll": "avkonfep_general.dll",
        "data/drives/z/rm-807/sys/bin/goommonitor.dll": (
            "goommonitor_general.dll"
        ),
    }
    assert set(changed) == set(replacements)
    assert set(missing) <= {"data/drives/z/rm-807/sys/bin/slpgw.dll"}
    for name, replacement in replacements.items():
        assert after[name] == _digest(instance / "patch" / replacement)
        assert after[name + ".bak"] == before[name]
    host, _, version, http2, reject = CASES[case]
    if reject:
        assert "TLS handshake failed" in result
    else:
        fields = dict(
            line.split("=", 1) for line in result.splitlines() if "=" in line
        )
        assert fields["host"] == host
        assert 200 <= int(fields["status"]) < 400
        assert int(fields["bytes"]) > 0
        assert int(fields["chunks"]) > 0
        if version:
            assert fields["tls"] == f"TLSv1.{version - 10}"
            assert fields["alpn"] == ("h2" if http2 else "http/1.1")
        if host == "example.com":
            assert "<!doctype html>" in fields["prefix"].lower()


@pytest.mark.parametrize(
    "http2,version", [(False, 0), (True, 0), (False, 12), (True, 13)]
)
def test_native_http_server(http_image, tmp_path, http2, version):
    """Checks native streaming uploads against independent host HTTP codecs."""
    import http.client
    import time

    from h2.config import H2Configuration
    from h2.connection import H2Connection
    from h2.events import DataReceived, ResponseReceived, StreamEnded

    golden = GOLDEN
    before = baseline_digest(golden)
    instance = tmp_path / "instance"
    shutil.copytree(golden, instance)
    drive = instance / "data/drives/rm-807/c"
    (drive / "sys/bin").mkdir(parents=True, exist_ok=True)
    shutil.copyfile(http_image, drive / "sys/bin/http_probe.exe")
    (drive / "http-case.txt").write_text(
        ("v" if http2 else "u") if version else ("t" if http2 else "s")
    )
    (drive / "http-result.txt").write_text("")
    (instance / "config.yml").write_text(
        "data-storage: data\ncpu: dynarmic\ndevice: 0\nlanguage: 1\n"
        "enable-gdb-stub: false\nlog-svc: false\n"
    )
    executable = EMULATOR
    with tempfile.TemporaryDirectory(prefix="native-http-", dir="/tmp") as p:
        control = Control(Path(p) / "control.sock")
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
                    executable_for_session(executable, Path(p)),
                    "--device",
                    "RM-807",
                    "--run",
                    "C:\\sys\\bin\\http_probe.exe",
                ],
                env=env,
                stdout=log,
                stderr=subprocess.STDOUT,
            )
            try:
                deadline = time.monotonic() + 20
                while True:
                    try:
                        transport = socket.create_connection(
                            ("127.0.0.1", 39105), timeout=1
                        )
                        break
                    except OSError:
                        assert time.monotonic() < deadline
                        time.sleep(0.05)
                if version:
                    fixtures = (
                        ROOT / "third_party/mbedtls-symbian/tests/fixtures"
                    )
                    context = ssl.create_default_context(
                        cafile=str(fixtures / "server-cert.pem")
                    )
                    context.load_cert_chain(
                        fixtures / "server-cert.pem",
                        fixtures / "server-key.pem",
                    )
                    context.minimum_version = context.maximum_version = (
                        ssl.TLSVersion.TLSv1_2
                        if version == 12
                        else ssl.TLSVersion.TLSv1_3
                    )
                    context.set_alpn_protocols(["h2" if http2 else "http/1.1"])
                    transport = context.wrap_socket(
                        transport, server_hostname="sdk-test"
                    )
                    assert transport.version() == f"TLSv1.{version - 10}"
                    assert transport.selected_alpn_protocol() == (
                        "h2" if http2 else "http/1.1"
                    )
                payload = b"streamed-upload" * 10000
                transport.settimeout(20)
                with transport:
                    if not http2:
                        client = http.client.HTTPConnection("127.0.0.1", 39105)
                        client.sock = transport
                        chunks = (
                            payload[i : i + 4096]
                            for i in range(0, len(payload), 4096)
                        )
                        client.request(
                            "POST", "/upload", chunks, encode_chunked=True
                        )
                        response = client.getresponse()
                        assert response.status == 200
                        body = response.read()
                    else:
                        client = H2Connection(
                            H2Configuration(
                                client_side=True, header_encoding="utf-8"
                            )
                        )
                        client.initiate_connection()
                        client.send_headers(
                            1,
                            [
                                (":method", "POST"),
                                (":scheme", "http"),
                                (":authority", "localhost"),
                                (":path", "/upload"),
                            ],
                        )
                        transport.sendall(client.data_to_send())
                        offset = 0
                        body = b""
                        ended = False
                        statuses = []
                        while not ended:
                            while offset < len(payload):
                                size = min(
                                    client.local_flow_control_window(1),
                                    client.max_outbound_frame_size,
                                    len(payload) - offset,
                                )
                                if not size:
                                    break
                                client.send_data(
                                    1,
                                    payload[offset : offset + size],
                                    end_stream=offset + size == len(payload),
                                )
                                offset += size
                            transport.sendall(client.data_to_send())
                            data = transport.recv(16384)
                            assert (
                                data
                            ), "HTTP/2 server closed before END_STREAM"
                            for event in client.receive_data(data):
                                if isinstance(event, ResponseReceived):
                                    statuses.append(
                                        dict(event.headers)[":status"]
                                    )
                                elif isinstance(event, DataReceived):
                                    body += event.data
                                    client.acknowledge_received_data(
                                        event.flow_controlled_length,
                                        event.stream_id,
                                    )
                                elif isinstance(event, StreamEnded):
                                    ended = True
                        assert statuses == ["200"]
                        assert offset == len(payload)
                assert body == f"native HTTP stream\n{len(payload)}\n".encode()
                assert process.wait(timeout=10) == 0
                exits = control.exit_report()["process_exits"]
                (tmp_path / "exits.json").write_text(json.dumps(exits))
                assert len(exits) == 1 and exits[0]["reason"] == 0, exits
            finally:
                _stop(process)
                assert baseline_digest(golden) == before
