"""Owned foreground launches using resolved, verified ROM / Z baselines."""

import argparse
import json
import os
import shutil
import signal
import socket
import subprocess
import sys
import tempfile
import time
import tomllib
from contextlib import contextmanager
from pathlib import Path

from pydantic import BaseModel, ConfigDict, Field

from symbian import toolchain
from symbian.e32 import inspect_image
from symbian.emulator import Control
from symbian.emulator.background import (
    background_environment,
    executable_for_session,
)
from symbian.emulator.configuration import add_options, options, resolve
from symbian.emulator.firmware import digest, selected, svc_profile
from symbian.status import Code, StatusError
from symbian.toolchain.host_tools import llvm_tool


def _digest(path: Path) -> str:
    return digest(path)


def _stop(process: subprocess.Popen | None) -> None:
    """Reaps only a subprocess owned by this launcher."""
    if process is None or process.poll() is not None:
        return
    process.terminate()
    try:
        process.wait(timeout=2)
    except subprocess.TimeoutExpired:
        process.kill()
        process.wait(timeout=5)


def guest_outcome(report: Path, uid: int) -> None:
    """Reports guest failure even when the frontend exits successfully."""
    if not report.is_file():
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Guest exit evidence unavailable: {report}",
        )
    try:
        envelope = json.loads(report.read_text())
        records = envelope["result"]["process_exits"]
        exits = [record for record in records if record["uid"] == uid]
        if not exits:
            raise KeyError("No matching application exit")
        failure = next(
            (record for record in exits if record["reason"] or record["type"]),
            None,
        )
    except (ValueError, KeyError, TypeError) as error:
        raise StatusError(
            Code.DATA_LOSS, f"Invalid guest exit evidence: {report}"
        ) from error
    if failure:
        code = {-4: Code.RESOURCE_EXHAUSTED, -5: Code.UNIMPLEMENTED}.get(
            failure["reason"], Code.FAILED_PRECONDITION
        )
        raise StatusError(
            code,
            f"Guest {failure['name']} exited with type "
            f"{failure['type']}, reason {failure['reason']}; "
            f"evidence: {report.parent}",
        )


class Session(BaseModel):
    """Resources of one fresh instance; its owning context reaps the child."""

    model_config = ConfigDict(arbitrary_types_allowed=True)

    process: subprocess.Popen = Field(exclude=True)
    directory: Path
    endpoint: Path
    symbols: Path | None = None
    source: Path | None = None
    headers: Path | None = None
    port: int = 24689
    image: dict = Field(default_factory=dict)
    name: str = "gui_app"

    def wait_ready(self, *, debug: bool, timeout: float = 20) -> None:
        """Waits without connecting to the single-client GDB listener."""
        deadline = time.monotonic() + timeout
        control = Control(self.endpoint, timeout=1)
        while time.monotonic() < deadline:
            if self.process.poll() is not None:
                raise StatusError(
                    Code.FAILED_PRECONDITION,
                    f"Emulator exited {self.process.returncode}; "
                    f"see {self.directory / 'frontend.log'}",
                )
            if debug:
                log = (self.directory / "frontend.log").read_text(
                    errors="replace"
                )
                if "Failed to bind gdb socket" in log:
                    raise StatusError(Code.ALREADY_EXISTS, "GDB port busy")
                if "Waiting for gdb" in log:
                    return
            else:
                try:
                    control.status()
                    return
                except StatusError as error:
                    if error.code != Code.UNAVAILABLE:
                        raise
            time.sleep(0.05)
        raise StatusError(Code.DEADLINE_EXCEEDED, "Emulator startup timed out")

    def gdb_hook(self) -> Path:
        """Relocates symbols after remote connection, before IDE breakpoints."""
        arguments = [
            str(self.directory),
            str(self.symbols),
            str(self.source),
            str(self.headers),
            self.image["code_base"],
            self.name,
            self.image["uid3"],
        ]
        hook = self.directory / "remote.gdb"
        package_root = str(Path(__file__).resolve().parents[2])
        hook.write_text(
            "set pagination off\nset confirm off\n"
            "set architecture arm\nset remotetimeout 20\n"
            "python\nimport sys\n"
            f"sys.path.insert(0, {package_root!r})\n"
            "from symbian.gdb_bridge import register\n"
            f"register(*{arguments!r})\nend\n"
            "define target hookpost-remote\n"
            "  symbian-relocate\n"
            "end\n",
            encoding="utf-8",
        )
        return hook


@contextmanager
def session(
    root: Path,
    *,
    debug: bool = False,
    port: int = 24689,
    project: Path | None = None,
    backend: str | None = None,
    overrides: dict | None = None,
    standalone: bool = False,
    sdk: Path | None = None,
):
    """Builds and launches one fixture copy, retaining logs and reaping it.

    Args:
        root: Prepared repository workspace for this bounded GUI application.
        debug: Starts the guest halted at its loopback GDB listener.
        port: Reserved configuration port for the IDE's remote connection.
        project: Optional project for application inputs or emulator settings.
        backend: Optional CPU backend override.
        overrides: Explicit emulator settings.
        standalone: Open the frontend without automatically running an app.
        sdk: Optional SDK used to resolve standalone emulator settings.

    Yields:
        Session with an owned child and private native control endpoint.
    """
    if standalone and debug:
        raise StatusError(
            Code.INVALID_ARGUMENT, "Standalone debug is unsupported"
        )
    from symbian.project.layout import has_application_manifest

    root = root.resolve()
    if standalone and project is None and has_application_manifest(root):
        project = root
    prepare_application = not standalone or (
        project is not None and has_application_manifest(project)
    )
    name, uid = "gui_app", 0xE0000811
    source = root / "examples/gui_app"
    build = root / ".symbian/gui-app"
    headers = root / ".symbian/gui-sdk/include"
    compiler, linker = None, None
    project_manifest = None
    if prepare_application:
        if project is not None:
            from symbian.project.configuration import ProjectConfiguration
            from symbian.project.sdk import AppSdk, discover_sdk

            project = project.resolve()
            generated = (project / "symbian-project.json").is_file()
            if generated:
                configuration = ProjectConfiguration.load(project)
                app_sdk = configuration.sdk
                port = configuration.preferences.port
            else:
                location = project / "sdk-location.json"
                if location.is_file():
                    sdk_path = Path(json.loads(location.read_text())["sdk"])
                    if not sdk_path.is_absolute():
                        sdk_path = (project / sdk_path).resolve()
                    app_sdk = AppSdk.load(discover_sdk(sdk_path))
                else:
                    app_sdk = AppSdk.load(discover_sdk())
            compiler, linker = str(app_sdk.compiler), str(app_sdk.linker)
            source, build = project, project / ".symbian/build"
            headers = app_sdk.prefix / "include/platform"
            project_manifest = tomllib.loads(
                (project / "symbian.toml").read_text()
            )
            options = project_manifest["project"]
            name, uid = options["name"], options["uid3"]
        if not source.is_dir():
            raise StatusError(
                Code.FAILED_PRECONDITION,
                f"Application source missing: {source}",
            )
        if compiler is None:
            selected_compiler = llvm_tool("clang++")
            compiler = str(selected_compiler)
            linker = str(llvm_tool("ld.lld", sibling=selected_compiler.parent))
    command = dict(overrides or {})
    if backend is not None:
        command["backend"] = backend
    resolution = resolve(project=project, sdk=sdk, root=root, overrides=command)
    golden, firmware = selected(resolution)
    profile = svc_profile(firmware, resolution.settings.profile)
    executable = resolution.settings.emulator
    backend = resolution.settings.backend or "dynarmic"
    language = resolution.settings.language or 1
    if prepare_application and firmware.device.kernel != "eka2":
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"{firmware.device.model} uses EKA1; the current ARM EABI/E32-V"
            " starter requires EKA2 startup/import ABI. Firmware import is"
            " supported; use toolchain verify-eka1 for the bounded no-UI"
            " Nokia 7610 profile. GUI adaptation remains unimplemented",
        )
    if backend not in ("dynarmic", "dyncom"):
        raise StatusError(Code.INVALID_ARGUMENT, "Unknown emulator CPU backend")
    inputs = {golden / path: value for path, value in firmware.files.items()}
    if prepare_application:
        for dll in ("euser.dll", "ws32.dll", "gdi.dll"):
            if not (
                golden / firmware.device.z_drive / "sys/bin" / dll
            ).is_file():
                raise StatusError(
                    Code.FAILED_PRECONDITION,
                    f"GUI requires {dll}; unavailable in selected firmware"
                    f" {firmware.identity}",
                )
    if executable is None or not executable.is_file():
        raise StatusError(
            Code.NOT_FOUND,
            f"Install an emulator with symbian emulator install: {executable}",
        )
    from symbian.emulator.distribution import check_packaged

    check_packaged(executable)
    if debug:
        if not 1024 <= port <= 65535:
            raise StatusError(Code.INVALID_ARGUMENT, "Invalid GDB port")
        try:
            with socket.socket() as reservation:
                reservation.bind(("127.0.0.1", port))
        except OSError as error:
            raise StatusError(Code.ALREADY_EXISTS, "GDB port busy") from error
    image = {}
    if prepare_application:
        toolchain.build(source, build, compiler, linker)
        image = inspect_image(build / f"{name}.exe")
        imported_dlls = {entry["dll"].lower() for entry in image["imports"]}
        pthread_path = firmware.device.z_drive + "/sys/bin/libpthread.dll"
        if (
            "libpthread.dll" in imported_dlls
            and pthread_path not in firmware.files
        ):
            raise StatusError(
                Code.FAILED_PRECONDITION,
                f"{firmware.device.model} has no libpthread.dll in drive Z, "
                "but "
                f"{name}.exe imports it. Generate with 'symbian init "
                "--portable-runtime' or set SYMBIAN_ENABLE_TIMER_TASKS=OFF "
                "and SYMBIAN_ENABLE_ABSEIL_STATUS=OFF, then rebuild.",
            )
        from symbian.toolchain.architecture import (
            require_execution_architecture,
        )

        require_execution_architecture(image["architecture"], backend)
        if image["uid3"] != uid or image["dll"]:
            raise StatusError(
                Code.FAILED_PRECONDITION, "Unexpected GUI executable"
            )
    output = (
        root / ".symbian/emulator-runs"
        if standalone
        else (
            (project / ".symbian/runs")
            if project
            else root / ".symbian/gui-runs"
        )
    )
    output.mkdir(parents=True, exist_ok=True)
    directory = Path(
        tempfile.mkdtemp(prefix="debug-" if debug else "run-", dir=output)
    )
    run_session_path = os.environ.get("SYMBIAN_CONSOLE_RUN_SESSION_PATH")
    if run_session_path:
        Path(run_session_path).write_text(str(directory), encoding="utf-8")
    instance = directory / "instance"
    shutil.copytree(golden, instance)
    application_assets = []
    if prepare_application:
        drive = instance / firmware.device.c_drive
        target = drive / "sys/bin" / f"{name}.exe"
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(build / f"{name}.exe", target)
        if (
            standalone
            and project_manifest
            and "application" in project_manifest
        ):
            from symbian.packaging.ca_bundle import selected_bundle
            from symbian.packaging.registration import compile_registration

            assets, _ = compile_registration(
                source,
                project_manifest["application"],
                f"{name}.exe",
                uid,
                sdk=app_sdk,
            )
            ca_bundle = selected_bundle(source, build / f"{name}.exe")
            if ca_bundle is not None:
                assets.append(
                    (f"!:\\resource\\apps\\{name}_ca.pem", ca_bundle[1])
                )
            for virtual_path, data in assets:
                parts = virtual_path.removeprefix("!:\\").split("\\")
                if not virtual_path.startswith("!:\\") or any(
                    part in {"", ".", ".."} or ":" in part for part in parts
                ):
                    raise StatusError(
                        Code.INVALID_ARGUMENT, "Unsafe application asset path"
                    )
                target = drive.joinpath(*parts)
                target.parent.mkdir(parents=True, exist_ok=True)
                target.write_bytes(data)
                application_assets.append(virtual_path)
    (instance / "config.yml").write_text(
        f"data-storage: data\ncpu: {backend}\ndevice: 0\nlanguage: {language}\n"
        f"enable-gdb-stub: {'true' if debug else 'false'}\n"
        f"gdb-port: {port}\nlog-svc: true\n"
    )
    manifest = {
        "schema": (
            "symbian.emulator-launch/v1"
            if standalone
            else "symbian.gui-launch/v1"
        ),
        "standalone": standalone,
        "application": (
            {
                "project": str(source),
                "name": name,
                "uid3": uid,
                "auto_run": not standalone,
                "assets": application_assets,
            }
            if prepare_application
            else None
        ),
        "debug": debug,
        "firmware": f"sha256:{firmware.identity}",
        "device": firmware.device.model_dump(),
        "configuration": resolution.model_dump(mode="json"),
        "svc_profile": profile,
        "gdb_port": port if debug else None,
        "inputs": {str(p): digest for p, digest in inputs.items()},
        "e32_sha256": (
            _digest(build / f"{name}.exe") if prepare_application else None
        ),
        "elf_sha256": (
            _digest(build / f"{name}.elf") if prepare_application else None
        ),
        "emulator_sha256": _digest(executable),
    }
    process = None
    try:
        with tempfile.TemporaryDirectory(
            prefix="symbian-ide-", dir="/tmp"
        ) as private:
            endpoint = Path(private) / "control.sock"
            env = os.environ.copy()
            env.pop("EKA2L1_EXPERIMENTAL_SVC_PROFILE", None)
            env.pop("QT_MAC_DISABLE_FOREGROUND_APPLICATION_TRANSFORM", None)
            env.update(
                EKA2L1_DATA_ROOT=str(instance),
                EKA2L1_RESEARCH_CONTROL_SOCKET=str(endpoint),
            )
            if env.get("SYMBIAN_CONSOLE_FOREGROUND_EMULATOR") == "1":
                env.pop("EKA2L1_RESEARCH_BACKGROUND_WINDOW", None)
            else:
                env.update(background_environment())
            if profile != "default":
                env["EKA2L1_EXPERIMENTAL_SVC_PROFILE"] = profile
            launch_executable = executable_for_session(executable, directory)
            with (directory / "frontend.log").open("w") as log:
                process = subprocess.Popen(
                    [
                        str(launch_executable),
                        "--device",
                        firmware.device.firmware_code,
                        *(
                            []
                            if standalone
                            else ["--run", f"C:\\sys\\bin\\{name}.exe"]
                        ),
                    ],
                    cwd=directory,
                    env=env,
                    stdout=log,
                    stderr=subprocess.STDOUT,
                )
                manifest.update(pid=process.pid, endpoint=str(endpoint))
                (directory / "launch.json").write_text(
                    json.dumps(manifest, indent=2) + "\n"
                )
                print(
                    f"Emulator session: {directory}",
                    file=sys.stderr,
                    flush=True,
                )
                active = Session(
                    process=process,
                    directory=directory,
                    endpoint=endpoint,
                    symbols=(
                        build / f"{name}.elf" if prepare_application else None
                    ),
                    source=source if prepare_application else None,
                    headers=headers if prepare_application else None,
                    name=name,
                    port=port,
                    image=image,
                )
                try:
                    active.wait_ready(debug=debug)
                    yield active
                finally:
                    _stop(process)
                    saved = endpoint.with_name(endpoint.name + ".status.json")
                    if saved.is_file():
                        shutil.copyfile(
                            saved, directory / "control.sock.status.json"
                        )
    finally:
        _stop(process)
        manifest["frontend_exit"] = process.returncode if process else None
        manifest["inputs_unchanged"] = all(
            _digest(p) == d for p, d in inputs.items()
        )
        (directory / "launch.json").write_text(
            json.dumps(manifest, indent=2) + "\n"
        )


def main(argv: list[str] | None = None, *, raise_errors: bool = False) -> int:
    """Launches Run or supervises the actual GDB child used by CLion Debug."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--root", type=Path, default=Path.cwd())
    parser.add_argument("--project", type=Path)
    parser.add_argument("--sdk", type=Path)
    parser.add_argument("--standalone", action="store_true")
    parser.add_argument("--gdb", type=Path)
    parser.add_argument("--port", type=int, default=24689)
    add_options(parser)
    args, debugger_args = parser.parse_known_args(argv)
    if args.gdb and any(
        a in ("--version", "--configuration", "--help") for a in debugger_args
    ):
        os.execv(str(args.gdb), [str(args.gdb), *debugger_args])
    if not args.gdb and debugger_args:
        parser.error("Unexpected Run arguments")

    def cancel(_signum, _frame):
        raise KeyboardInterrupt

    signal.signal(signal.SIGTERM, cancel)
    debugger = None
    try:
        with session(
            args.root,
            debug=bool(args.gdb),
            port=args.port,
            project=args.project,
            overrides=options(args),
            standalone=args.standalone,
            sdk=args.sdk,
        ) as active:
            if args.gdb:
                debugger = subprocess.Popen(
                    [
                        str(args.gdb),
                        "-x",
                        str(active.gdb_hook()),
                        *debugger_args,
                    ]
                )
                result = debugger.wait()
            else:
                result = active.process.wait()
        if not args.standalone and not args.gdb and result == 0:
            guest_outcome(
                active.directory / "control.sock.status.json",
                active.image["uid3"],
            )
        return result
    except KeyboardInterrupt:
        return 130
    except (StatusError, OSError) as error:
        if raise_errors:
            if isinstance(error, StatusError):
                raise
            raise StatusError(Code.INTERNAL, str(error)) from error
        print(str(error), file=sys.stderr)
        return 1
    finally:
        _stop(debugger)


if __name__ == "__main__":
    sys.exit(main())
