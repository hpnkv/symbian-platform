"""Entry point for read-only research and Symbian application builds."""

import argparse
import os
import re
import sys
from pathlib import Path

from symbian.cli.output import render
from symbian.doctor import doctor
from symbian.emulator.configuration import (
    add_options,
    option_arguments,
    options,
)


def _help_colour_enabled() -> bool:
    """Use terminal colour unless the user or terminal disables it."""
    if "NO_COLOR" in os.environ or os.environ.get("TERM") == "dumb":
        return False
    return sys.stdout.isatty() or os.environ.get("CLICOLOR_FORCE") not in (
        None,
        "0",
    )


def _help_style(value: str, code: str) -> str:
    """Wrap one help fragment in ANSI style when colour is active."""
    return f"\x1b[{code}m{value}\x1b[0m"


class StyledArgumentParser(argparse.ArgumentParser):
    """Keep argparse help structure and colour its visible hierarchy."""

    def add_subparsers(self, **arguments):
        arguments.setdefault("parser_class", type(self))
        return super().add_subparsers(**arguments)

    def format_help(self) -> str:
        plain_help = super().format_help()
        if not _help_colour_enabled():
            return plain_help
        styled_lines = []
        for line in plain_help.splitlines(keepends=True):
            if line.startswith("usage: "):
                line = _help_style("usage:", "1;36") + line[len("usage:") :]
            elif (
                line.strip()
                and not line[0].isspace()
                and line.rstrip().endswith(":")
            ):
                line = _help_style(line.rstrip(), "1;36") + "\n"
            elif line.strip() == self.description:
                line = _help_style(line.rstrip(), "1") + "\n"
            elif line.strip() == self.epilog:
                line = _help_style(line.rstrip(), "2") + "\n"
            else:
                action = re.match(r"^(  +)(\S.*?)( {2,})(\S.*)$", line)
                if action is not None:
                    line = (
                        action.group(1)
                        + _help_style(action.group(2), "1;32")
                        + action.group(3)
                        + action.group(4)
                        + ("\n" if line.endswith("\n") else "")
                    )
            styled_lines.append(line)
        return "".join(styled_lines)


class SymbianHelpFormatter(argparse.ArgumentDefaultsHelpFormatter):
    """Show meaningful defaults without repeating absent flag values."""

    def _get_help_string(self, action: argparse.Action) -> str:
        if action.default is None or action.default is False:
            return action.help or ""
        return super()._get_help_string(action)


_COMMAND_DESCRIPTIONS = {
    (): "Build software, inspect inputs, and manage devices and emulators.",
    ("doctor",): "Check host tools and verified target capabilities.",
    ("init",): "Create a standalone application and its initial build.",
    ("app",): "Build, run, or configure a standalone application.",
    (
        "app",
        "build",
    ): "Build the project with its selected SDK and verify reproducibility.",
    (
        "app",
        "run",
    ): "Build and run the application in a selected emulator instance.",
    (
        "app",
        "configure",
    ): "Regenerate SDK and IDE configuration for an existing project.",
    ("sdk",): "Install a visible SDK directory that applications can select.",
    (
        "sdk",
        "install",
    ): "Copy an installed SDK or export one from a prepared workspace.",
    (
        "firmware",
    ): "Import, inspect, and transfer ROM and drive-Z configurations.",
    (
        "firmware",
        "list",
    ): "List shared firmware imports available from the configured store.",
    (
        "firmware",
        "inspect",
    ): "Inspect one imported firmware identity and device profile.",
    (
        "firmware",
        "probe",
    ): "Examine an input archive without importing or changing it.",
    (
        "firmware",
        "import",
    ): "Import a ROM/Z source into the independent content store.",
    (
        "firmware",
        "export",
    ): "Make a portable bundle of one imported firmware identity.",
    ("emu",): "Resolve emulator settings and inspect a running instance.",
    (
        "emu",
        "resolve",
    ): "Show effective global, SDK, project, and command overrides.",
    ("emu", "configure"): "Save emulator settings at one configuration level.",
    ("emu", "status"): "Inspect a running emulator through its control socket.",
    ("device",): "Discover USB handsets and stage application packages safely.",
    (
        "agent",
    ): "Talk to a manually addressed, authenticated development agent.",
    ("agent", "status"): "Read the agent's current status over mutual TLS.",
    ("agent", "logs"): "Read bounded service events after a sequence cursor.",
    (
        "device",
        "list",
    ): "List candidate Symbian handsets associated with the host.",
    ("device", "info"): "Inspect USB and mounted-volume state for one handset.",
    (
        "device",
        "install",
    ): "Build and stage a SIS for a human-approved on-phone install.",
    (
        "device",
        "policy",
    ): "Explain whether a device operation has an authorized executor.",
    (
        "inspect",
    ): "Read ELF, E32, SIS, or import-proxy metadata from a local file.",
    ("build",): "Build an ARM/E32 application executable.",
    ("package",): "Package an application executable and resources into a SIS.",
    ("preserve",): "Create or verify digested copies of research inputs.",
    ("toolchain",): "Build and validate ARM/E32 application artifacts.",
    ("console",): "Open the graphical SDK and device console.",
}

_OPTION_DESCRIPTIONS = {
    "project": "Application project directory (default: current directory).",
    "sdk": "Installed SDK directory or its sdk.json path.",
    "root": "Workspace root for resolving relative configuration paths.",
    "workspace": "Prepared source workspace for a fresh SDK export.",
    "output": "Destination for generated artifacts or verification reports.",
    "compiler": "ARM-capable Clang C++ compiler executable.",
    "linker": "LLD linker executable.",
    "architecture": "Target instruction-set profile; ARMv6 is the default.",
    "device": "USB selector shown by symbian device list.",
    "volume": "Mounted disk identifier shown by symbian device info.",
    "package": "Input SIS package file.",
    "endpoint": "Control socket of the owned emulator instance.",
    "timeout": "Maximum wait time in seconds.",
    "format": "Input artifact format to inspect.",
    "non_interactive": "Use supplied options and defaults without prompting.",
    "name": "Name for the created application or imported item.",
    "ide": "Generate IntelliJ/CLion project integration, or omit it.",
    "uid3": "Application or DLL UID3, written in decimal or 0x notation.",
    "scope": "Configuration level to update.",
    "unset": "Setting name to remove; may be repeated.",
    "clear_firmware": "Remove the saved firmware selection at this level.",
    "artifact": "Local binary artifact to read or verify.",
    "source": "Input archive, ROM image, or source directory.",
    "destination": "Directory or file to create.",
    "reference": "Firmware alias or sha256: content identity.",
    "operation": "Device operation to inspect without executing it.",
    "definition": "Frozen Symbian DEF export definition.",
    "target_dll": "Target DLL name used by the import proxy.",
    "symbol": "Selected exported symbol; may be repeated.",
    "executable": "Associated E32 executable for independent validation.",
    "oracles_build": "Built EKA2L1 validation and execution tools.",
    "provenance": "Short description stored with the preservation manifest.",
    "manifest_sha256": "Expected manifest digest for independent verification.",
    "read_only": "Report without making changes.",
    "store": "Shared content store for imported ROM and drive-Z material.",
    "emulator": "EKA2L1 frontend executable to use for this command.",
    "importer": "Native EKA2L1 firmware import helper executable.",
    "backend": "Emulator CPU backend for this run.",
    "language": "Emulated system language number.",
    "profile": "Compatibility profile for the selected emulator firmware.",
    "headers": "Prepared platform header directory for symbol validation.",
    "elf": "Linked ARM ELF input image.",
    "import_proxy": "Selected import-proxy library; may be repeated.",
    "sources_root": "Pinned original Symbian source checkout.",
    "gdb": "ARM GDB executable for the IDE's remote debug profile.",
    "saved": "Read the retained exit report from a stopped emulator session.",
    "x": "Horizontal pointer coordinate in display pixels.",
    "y": "Vertical pointer coordinate in display pixels.",
    "action": "Pointer button transition to send.",
    "rom": "Unpacked ROM image input.",
    "vpl": "Firmware VPL manifest input.",
    "instance": "Existing EKA2L1 instance directory to import.",
    "bundle": "Portable firmware bundle to import.",
    "rpkg": "RPKG companion for a ROM image.",
    "z_drive": "Unpacked drive-Z companion for a ROM image.",
    "variant": "ROM variant index; -1 selects the importer default.",
    "replace_alias": "Move an existing local alias to the imported identity.",
    "archive": "Preserved archive directory to create or verify.",
    "host": "Explicit IP address or host name of the running agent.",
    "port": "TCP port of the running agent.",
    "server_name": "Name that must match the agent's server certificate.",
    "ca_bundle": "PEM roots trusted for this agent connection.",
    "client_certificate": "PEM certificate presented to the agent.",
    "client_key": "Private key matching the client certificate.",
}


def _add_output_format(
    parser: argparse.ArgumentParser, path: tuple[str, ...] = ()
) -> None:
    """Adds presentation and descriptive help at every command level."""
    parser.formatter_class = lambda prog: SymbianHelpFormatter(
        prog, max_help_position=32, width=96
    )
    parser.description = _COMMAND_DESCRIPTIONS.get(
        path,
        parser.description
        or "Inspect or change the selected Symbian workflow.",
    )
    parser.epilog = (
        "Use --output-format=json for stable machine-readable output."
    )
    parser.add_argument(
        "--output-format",
        choices=("human", "json"),
        default="human" if not path else argparse.SUPPRESS,
        help="Display a readable summary (default) or canonical JSON",
    )
    for action in parser._actions:
        if action.help is None and action.dest in _OPTION_DESCRIPTIONS:
            action.help = _OPTION_DESCRIPTIONS[action.dest]
        if (
            path == ("toolchain", "prepare-gui-sdk")
            and action.dest == "profile"
        ):
            action.help = "Explicit source-input selection and digest profile."
        if isinstance(action, argparse._SubParsersAction):
            listed = {choice.dest: choice for choice in action._choices_actions}
            action._choices_actions[:] = [
                listed.get(command)
                or action._ChoicesPseudoAction(
                    command,
                    [],
                    help=_COMMAND_DESCRIPTIONS.get(
                        path + (command,),
                        "Inspect or change this Symbian workflow.",
                    ),
                )
                for command in action.choices
            ]
            for choice in action._choices_actions:
                if not choice.help:
                    choice.help = _COMMAND_DESCRIPTIONS.get(
                        path + (choice.dest,),
                        "Inspect or change this Symbian workflow.",
                    )
            for command, child in action.choices.items():
                _add_output_format(child, path + (command,))


def _parser() -> argparse.ArgumentParser:
    parser = StyledArgumentParser(prog="symbian")
    commands = parser.add_subparsers(dest="command", required=True)
    commands.add_parser("doctor", help="Report host tools and target readiness")
    console = commands.add_parser(
        "console", help="Open the graphical SDK and device console"
    )
    console.add_argument(
        "--workdir",
        type=Path,
        help="Working directory for project and SDK discovery",
    )
    init = commands.add_parser(
        "init", help="Create a complete hello-time app project"
    )
    init.add_argument("destination", type=Path)
    init.add_argument("--sdk", type=Path, default=None)
    init.add_argument("--name")
    init.add_argument("--ide", choices=("intellij", "none"))
    init.add_argument("--architecture", choices=("armv6", "armv5t"))
    init.add_argument("--uid3", type=lambda value: int(value, 0))
    init.add_argument("--non-interactive", action="store_true")
    init.add_argument(
        "--portable-runtime",
        action="store_true",
        help="Use the GUI/heap profile without the libpthread-backed task APIs",
    )
    add_options(init)
    init.add_argument(
        "--no-build", action="store_true", help="Skip initial build"
    )
    app = commands.add_parser("app").add_subparsers(
        dest="app_command", required=True
    )
    for action in ("build", "run"):
        operation = app.add_parser(action)
        operation.add_argument("--project", type=Path, default=Path.cwd())
        if action == "run":
            add_options(operation)
    export = commands.add_parser(
        "prepare-app-sdk", help="Export a local target SDK"
    )
    export.add_argument("--workspace", type=Path, default=Path.cwd())
    export.add_argument("--output", type=Path, default=Path(".symbian/app-sdk"))
    sdk_commands = commands.add_parser("sdk").add_subparsers(
        dest="sdk_command", required=True
    )
    install = sdk_commands.add_parser(
        "install", help="Install an observable SDK directory"
    )
    install.add_argument("destination", type=Path)
    install.add_argument("--workspace", type=Path)
    agent_commands = commands.add_parser("agent").add_subparsers(
        dest="agent_command", required=True
    )
    for name in ("status", "logs"):
        agent_parser = agent_commands.add_parser(name)
        agent_parser.add_argument("host")
        agent_parser.add_argument("port", type=int)
        agent_parser.add_argument("--server-name", required=True)
        agent_parser.add_argument("--ca-bundle", required=True, type=Path)
        agent_parser.add_argument(
            "--client-certificate", required=True, type=Path
        )
        agent_parser.add_argument("--client-key", required=True, type=Path)
        agent_parser.add_argument("--timeout", type=float, default=5.0)
        if name == "logs":
            agent_parser.add_argument(
                "--after",
                type=int,
                default=0,
                help="Return records after this sequence (default: 0).",
            )
            agent_parser.add_argument(
                "--limit",
                type=int,
                default=8,
                help="Return 1–8 records (default: 8).",
            )
    configure = app.add_parser(
        "configure", help="Refresh SDK and CLion integration"
    )
    configure.add_argument("--project", type=Path, default=Path.cwd())
    configure.add_argument("--sdk", type=Path)
    compiler_commands = commands.add_parser("toolchain").add_subparsers(
        dest="toolchain_command", required=True
    )
    probe = compiler_commands.add_parser(
        "probe", help="Test ARM ELF generation"
    )
    probe.add_argument("--output", type=Path, default=Path(".symbian/probe"))
    probe.add_argument("--compiler", default="clang++")
    probe.add_argument(
        "--architecture", choices=("armv6", "armv5t"), default="armv6"
    )
    verify_probe = compiler_commands.add_parser(
        "verify-probe", help="Run independent E32, CPU and kernel checks"
    )
    verify_probe.add_argument("artifact", type=Path)
    verify_probe.add_argument(
        "--oracles-build", type=Path, default=Path("build/eka2l1")
    )
    verify_probe.add_argument(
        "--output", type=Path, default=Path(".symbian/probe-check")
    )
    pointers = compiler_commands.add_parser(
        "verify-pointers",
        help="Check relocated callbacks and C++ virtual dispatch",
    )
    pointers.add_argument("artifact", type=Path)
    pointers.add_argument("--package", type=Path)
    pointers.add_argument(
        "--oracles-build", type=Path, default=Path("build/eka2l1")
    )
    pointers.add_argument(
        "--output", type=Path, default=Path(".symbian/pointer-check")
    )
    verify_package = compiler_commands.add_parser(
        "verify-package", help="Check SIS installation and kernel execution"
    )
    verify_package.add_argument("package", type=Path)
    verify_package.add_argument("--executable", type=Path, required=True)
    verify_package.add_argument(
        "--oracles-build", type=Path, default=Path("build/eka2l1")
    )
    verify_package.add_argument(
        "--output", type=Path, default=Path(".symbian/package-check")
    )
    proxy = compiler_commands.add_parser(
        "import-proxy", help="Build selected DEF ordinal proxies"
    )
    proxy.add_argument("definition", type=Path)
    proxy.add_argument("--symbol", action="append", required=True)
    proxy.add_argument("--target-dll", required=True)
    proxy.add_argument(
        "--output", type=Path, default=Path(".symbian/import-proxy")
    )
    proxy.add_argument("--compiler", default="clang++")
    proxy.add_argument("--linker", default="ld.lld")
    proxy.add_argument("--headers", type=Path)
    dll_convert = compiler_commands.add_parser(
        "convert-dll", help="Convert a linked ARM ELF and frozen DEF to E32 DLL"
    )
    dll_convert.add_argument("elf", type=Path)
    dll_convert.add_argument("--definition", type=Path, required=True)
    dll_convert.add_argument(
        "--uid3", type=lambda value: int(value, 0), required=True
    )
    dll_convert.add_argument(
        "--import-proxy", type=Path, action="append", default=[]
    )
    dll_convert.add_argument("--output", type=Path, required=True)
    gui_sdk = compiler_commands.add_parser(
        "prepare-gui-sdk", help="Stage the GUI example's pinned source SDK"
    )
    gui_sdk.add_argument("--profile", type=Path, required=True)
    gui_sdk.add_argument("--sources-root", type=Path, required=True)
    gui_sdk.add_argument(
        "--output", type=Path, default=Path(".symbian/gui-sdk")
    )
    gui = compiler_commands.add_parser(
        "verify-gui", help="Validate the GUI image without running its imports"
    )
    gui.add_argument("artifact", type=Path)
    gui.add_argument("--oracles-build", type=Path, default=Path("build/eka2l1"))
    gui.add_argument("--output", type=Path, default=Path(".symbian/gui-check"))
    gui_package = compiler_commands.add_parser(
        "verify-gui-package", help="Check GUI SIS installation without OS boot"
    )
    gui_package.add_argument("package", type=Path)
    gui_package.add_argument("--executable", type=Path, required=True)
    gui_package.add_argument(
        "--oracles-build", type=Path, default=Path("build/eka2l1")
    )
    gui_package.add_argument(
        "--output", type=Path, default=Path(".symbian/gui-package-check")
    )
    emulator_commands = commands.add_parser("emu").add_subparsers(
        dest="emu_command", required=True
    )
    for action in ("resolve", "configure"):
        command = emulator_commands.add_parser(action)
        command.add_argument("--project", type=Path)
        command.add_argument("--sdk", type=Path)
        command.add_argument("--root", type=Path, default=Path.cwd())
        add_options(command)
        if action == "configure":
            command.add_argument(
                "--scope", choices=("global", "sdk", "project"), required=True
            )
            command.add_argument("--unset", action="append", default=[])
            command.add_argument("--clear-firmware", action="store_true")
    firmware_commands = commands.add_parser(
        "firmware", help="Import and select shared ROM / drive Z baselines"
    ).add_subparsers(dest="firmware_command", required=True)
    for action in ("import", "list", "inspect", "export", "probe"):
        command = firmware_commands.add_parser(action)
        command.add_argument("--project", type=Path)
        command.add_argument("--sdk", type=Path)
        command.add_argument("--root", type=Path, default=Path.cwd())
        add_options(command, firmware=False)
        if action == "probe":
            command.add_argument("source", type=Path)
        elif action == "import":
            command.add_argument(
                "source",
                nargs="?",
                type=Path,
                help=".7z/.zip ROM/RPKG or ROM/Z archive",
            )
            group = command.add_mutually_exclusive_group()
            for form in ("rom", "vpl", "instance", "bundle"):
                group.add_argument(f"--{form}", type=Path)
            companion = command.add_mutually_exclusive_group()
            companion.add_argument("--rpkg", type=Path)
            companion.add_argument("--z-drive", type=Path)
            command.add_argument("--variant", type=int, default=-1)
            command.add_argument("--name")
            command.add_argument("--replace-alias", action="store_true")
            command.add_argument(
                "--use",
                choices=("global", "sdk", "project"),
                help="Save the imported exact content ID at this scope",
            )
            command.add_argument("--timeout", type=float, default=300)
        elif action in ("inspect", "export"):
            command.add_argument("reference", help="Alias or sha256:ID")
            if action == "export":
                command.add_argument("destination", type=Path)
    ide = emulator_commands.add_parser(
        "configure-ide", help="Install local GUI Run and ARM Debug profiles"
    )
    ide.add_argument("--root", type=Path, default=Path.cwd())
    ide.add_argument("--gdb", type=Path)
    for operation in ("status", "screenshot", "pointer"):
        command = emulator_commands.add_parser(operation)
        command.add_argument("--endpoint", type=Path, required=True)
        command.add_argument("--timeout", type=float, default=5)
        if operation == "status":
            command.add_argument("--saved", action="store_true")
        elif operation == "screenshot":
            command.add_argument("--name", required=True)
        elif operation == "pointer":
            command.add_argument("x", type=int)
            command.add_argument("y", type=int)
            command.add_argument("action", choices=("press", "release"))
    build = commands.add_parser(
        "build", help="Build an ARM/E32 application executable"
    )
    build.add_argument("--project", type=Path, default=Path.cwd())
    build.add_argument("--output", type=Path, default=Path(".symbian/build"))
    build.add_argument("--compiler", default="clang++")
    build.add_argument("--linker", default="ld.lld")
    build.add_argument("--architecture", choices=("armv6", "armv5t"))
    package = commands.add_parser(
        "package", help="Build an unsigned SISX application package"
    )
    package.add_argument("--project", type=Path, default=Path.cwd())
    package.add_argument("--artifact", type=Path, required=True)
    package.add_argument(
        "--output", type=Path, default=Path(".symbian/package")
    )
    inspect = commands.add_parser(
        "inspect", help="Inspect native format metadata"
    )
    inspect.add_argument("artifact", type=Path)
    inspect.add_argument(
        "--format",
        choices=("elf32", "e32", "sis", "import-proxy"),
        default="elf32",
    )
    preserve_commands = commands.add_parser("preserve").add_subparsers(
        dest="preserve_command", required=True
    )
    create = preserve_commands.add_parser(
        "create", help="Copy and seal artifacts"
    )
    create.add_argument("source", type=Path)
    create.add_argument("archive", type=Path)
    create.add_argument("--provenance", default="unknown")
    verify = preserve_commands.add_parser("verify", help="Check archive hashes")
    verify.add_argument("archive", type=Path)
    verify.add_argument("--manifest-sha256")
    device_commands = commands.add_parser("device").add_subparsers(
        dest="device_command", required=True
    )
    policy = device_commands.add_parser(
        "policy", help="Describe operation authority"
    )
    policy.add_argument("operation")
    device_commands.add_parser(
        "list", help="List connected USB Symbian device candidates"
    )
    info = device_commands.add_parser(
        "info", help="Inspect USB descriptors and mounted storage"
    )
    info.add_argument("--device")
    info.add_argument(
        "--no-protocol",
        action="store_true",
        help="Show host USB evidence without querying the CDC ACM port",
    )
    info.add_argument(
        "--usb-map",
        action="store_true",
        help="Compatibility alias; the USB map is always included",
    )
    info.add_argument(
        "--at-status",
        action="store_true",
        help="Query battery and signal codes from the AT modem port",
    )
    info.add_argument(
        "--mtp",
        action="store_true",
        help="Read MTP device and storage information",
    )
    info.add_argument(
        "--mtp-list",
        type=int,
        metavar="LIMIT",
        default=0,
        help="Also list up to LIMIT root object handles per MTP storage",
    )
    info.add_argument(
        "--obex-connect",
        action="store_true",
        help="Attempt and close a PC Suite OBEX session",
    )
    mode_commands = device_commands.add_parser(
        "mode", help="Observe a human-selected USB mode transition"
    ).add_subparsers(dest="mode_command", required=True)
    mode_begin = mode_commands.add_parser(
        "begin", help="Save the current USB mode as a verification baseline"
    )
    mode_begin.add_argument("--device", help="Exact selector from device list")
    mode_begin.add_argument(
        "--ticket", required=True, type=Path, help="New private baseline file"
    )
    mode_verify = mode_commands.add_parser(
        "verify", help="Check the phone after its USB mode changes"
    )
    mode_verify.add_argument(
        "--ticket", required=True, type=Path, help="Saved baseline file"
    )
    install_app = device_commands.add_parser(
        "install",
        help="Build, package and stage a SIS for on-phone installation",
    )
    install_app.add_argument("--project", type=Path, default=Path.cwd())
    install_app.add_argument("--device")
    install_app.add_argument(
        "--volume", help="Disk identifier from device info"
    )
    install_app.add_argument("--package", type=Path, help="Use a prepared SIS")
    install_app.add_argument("--compiler", default="clang++")
    install_app.add_argument("--linker", default="ld.lld")
    _add_output_format(parser)
    return parser


def _execute(args: argparse.Namespace) -> dict:
    from symbian import device, packaging, preservation, toolchain
    from symbian.analysis import inspect_elf
    from symbian.e32 import inspect_image

    if args.command == "doctor":
        return doctor()
    if args.command == "agent":
        from symbian.agent import ReadOnlyAgentSession

        with ReadOnlyAgentSession.connect(
            args.host,
            args.port,
            server_name=args.server_name,
            ca_bundle=args.ca_bundle,
            client_certificate=args.client_certificate,
            client_key=args.client_key,
            timeout=args.timeout,
        ) as agent:
            if args.agent_command == "logs":
                return agent.logs(
                    after=args.after, limit=args.limit
                ).model_dump()
            return agent.status().model_dump()
    if args.command == "firmware":
        from symbian.emulator.configuration import (
            config_path,
            configure,
            resolve,
        )
        from symbian.emulator.firmware import (
            export_firmware,
            import_firmware,
            list_firmware,
            locate,
            validate_manifest,
        )
        from symbian.status import Code, StatusError

        resolution = resolve(
            project=args.project,
            sdk=args.sdk,
            overrides=options(args),
            root=args.root.resolve(),
        )
        store = resolution.settings.store
        if store is None:
            raise StatusError(
                Code.FAILED_PRECONDITION, "No firmware store configured"
            )
        if args.firmware_command == "list":
            return list_firmware(store)
        if args.firmware_command == "probe":
            from symbian.emulator.firmware import probe_archive

            return probe_archive(resolution, args.source)
        if args.firmware_command == "inspect":
            return validate_manifest(locate(store, args.reference)).model_dump(
                mode="json"
            )
        if args.firmware_command == "export":
            return export_firmware(store, args.reference, args.destination)
        forms = [
            (key, getattr(args, key))
            for key in ("rom", "vpl", "instance", "bundle")
            if getattr(args, key) is not None
        ]
        if (args.source is None) == (not forms):
            raise StatusError(
                Code.INVALID_ARGUMENT,
                "Supply one archive SOURCE or one of --rom / --vpl / --instance"
                " / --bundle",
            )
        form, source = forms[0] if forms else ("archive", args.source)
        if (args.rpkg or args.z_drive) and form != "rom":
            raise StatusError(
                Code.INVALID_ARGUMENT, "--rpkg / --z-drive require --rom"
            )
        if args.timeout <= 0 or args.variant < -1:
            raise StatusError(
                Code.INVALID_ARGUMENT,
                "Positive timeout and variant >= -1 required",
            )
        if args.use:
            config_path(args.use, project=args.project, sdk=args.sdk)
        result = import_firmware(
            resolution,
            source=source,
            form="z" if args.z_drive else form,
            companion=args.z_drive or args.rpkg,
            name=args.name,
            replace_alias=args.replace_alias,
            variant=args.variant,
            timeout=args.timeout,
        )
        if args.use:
            values = {"firmware": result["firmware"]}
            if args.store is not None:
                values["store"] = str(args.store.resolve())
            result["configuration"] = configure(
                args.use, values, project=args.project, sdk=args.sdk
            )
        return result
    if args.command == "sdk":
        from symbian.project.sdk import install

        return install(args.destination, args.workspace).model_dump(mode="json")
    if args.command == "prepare-app-sdk":
        from symbian.project.sdk import activate_sdk, install_tools, prepare

        sdk = install_tools(prepare(args.workspace, args.output))
        activate_sdk(sdk)
        return sdk.model_dump(mode="json")
    if args.command == "init":
        import os

        from symbian.project.generate import wizard
        from symbian.project.sdk import AppSdk, discover_sdk

        sdk_path = discover_sdk(args.sdk)
        selected = AppSdk.load(sdk_path)
        entry = selected.prefix / "bin/symbian"
        if entry.is_file() and os.environ.get("SYMBIAN_ACTIVE_SDK") != str(
            selected.prefix
        ):
            arguments = [
                str(entry),
                "init",
                str(args.destination),
                "--sdk",
                str(sdk_path),
            ]
            for flag, value in (
                ("--name", args.name),
                ("--ide", args.ide),
                ("--uid3", args.uid3),
                ("--architecture", args.architecture),
            ):
                if value is not None:
                    arguments.extend((flag, str(value)))
            for flag, enabled in (
                ("--non-interactive", args.non_interactive),
                ("--no-build", args.no_build),
                ("--portable-runtime", args.portable_runtime),
            ):
                if enabled:
                    arguments.append(flag)
            arguments.extend(option_arguments(args))
            arguments.extend(("--output-format", args.output_format))
            os.execv(str(entry), arguments)

        emulator_values = {}
        timer_tasks = not args.portable_runtime
        if options(args):
            from symbian.emulator.configuration import resolve
            from symbian.emulator.firmware import selected

            resolved = resolve(sdk=sdk_path, overrides=options(args))
            emulator_values = {
                key: str(value.resolve()) if isinstance(value, Path) else value
                for key, value in options(args).items()
            }
            if "firmware" in emulator_values:
                _, imported = selected(resolved)
                emulator_values["firmware"] = f"sha256:{imported.identity}"
                if (
                    imported.device.z_drive + "/sys/bin/libpthread.dll"
                    not in imported.files
                ):
                    timer_tasks = False
        result = wizard(
            args.destination,
            sdk_path,
            name=args.name,
            ide=args.ide,
            uid3=args.uid3,
            architecture=args.architecture,
            timer_tasks=timer_tasks,
            non_interactive=args.non_interactive,
        )
        if emulator_values:
            from symbian.emulator.configuration import configure

            project = Path(result["project"])
            result["emulator_configuration"] = configure(
                "project", emulator_values, project=project
            )
        if not args.no_build:
            from symbian.process import run
            from symbian.project.configuration import ProjectConfiguration

            project = Path(result["project"])
            sdk = ProjectConfiguration.load(project).sdk
            run(
                [
                    str(sdk.prefix / "bin/symbian"),
                    "app",
                    "build",
                    "--project",
                    str(project),
                ],
                cwd=project,
            )
            result["initial_build"] = True
        return result
    if args.command == "app":
        from symbian.project.configuration import ProjectConfiguration

        project = args.project.resolve()
        if (
            args.app_command == "run"
            and not (project / "symbian-project.json").is_file()
        ):
            from symbian.emulator.launch import main as launch
            from symbian.status import Code, StatusError

            result = launch(
                ["--project", str(project), *option_arguments(args)],
                raise_errors=True,
            )
            if result:
                raise StatusError(
                    Code.CANCELLED if result == 130 else Code.INTERNAL,
                    f"Emulator supervisor exited {result}",
                )
            return {"frontend_exit": result}
        configuration = ProjectConfiguration.load(project)
        if args.app_command == "configure":
            from symbian.project.generate import configure_project
            from symbian.project.sdk import AppSdk

            if args.sdk:
                sdk = AppSdk.load(
                    args.sdk / "sdk.json" if args.sdk.is_dir() else args.sdk
                )
            else:
                sdk = configuration.sdk
            return configure_project(project, configuration.preferences, sdk)
        sdk = configuration.sdk
        import os

        if os.environ.get("SYMBIAN_ACTIVE_SDK") != str(sdk.prefix):
            os.execv(
                str(sdk.prefix / "bin/symbian"),
                [
                    str(sdk.prefix / "bin/symbian"),
                    "app",
                    args.app_command,
                    "--project",
                    str(project),
                    "--output-format",
                    args.output_format,
                    *option_arguments(args),
                ],
            )
        if args.app_command == "build":
            return toolchain.build(
                project,
                project / ".symbian/build",
                str(sdk.compiler),
                str(sdk.linker),
            )
        from symbian.emulator.launch import main as launch
        from symbian.status import Code, StatusError

        result = launch(
            ["--project", str(project), *option_arguments(args)],
            raise_errors=True,
        )
        if result:
            raise StatusError(
                Code.CANCELLED if result == 130 else Code.INTERNAL,
                f"Emulator supervisor exited {result}",
            )
        return {"frontend_exit": result}
    if args.command == "toolchain":
        if args.toolchain_command == "verify-gui-package":
            from symbian.packaging.verification import verify_gui_package

            return verify_gui_package(
                args.package, args.executable, args.oracles_build, args.output
            )
        if args.toolchain_command == "verify-gui":
            from symbian.toolchain.verification import verify_gui

            return verify_gui(args.artifact, args.oracles_build, args.output)
        if args.toolchain_command == "prepare-gui-sdk":
            from symbian.sdk.staging import prepare_gui_sdk

            return prepare_gui_sdk(args.profile, args.sources_root, args.output)
        if args.toolchain_command == "probe":
            return toolchain.probe(
                args.output, args.compiler, architecture=args.architecture
            )
        if args.toolchain_command == "import-proxy":
            from symbian.sdk import build_import_proxy

            return build_import_proxy(
                args.definition,
                args.symbol,
                args.target_dll,
                args.output,
                args.compiler,
                args.linker,
                args.headers,
            )
        if args.toolchain_command == "convert-dll":
            from symbian.e32 import convert_dll, inspect_image

            image = convert_dll(
                args.elf.read_bytes(),
                args.definition.read_bytes(),
                [proxy.read_bytes() for proxy in args.import_proxy],
                args.uid3,
            )
            args.output.parent.mkdir(parents=True, exist_ok=True)
            staged = args.output.with_suffix(args.output.suffix + ".tmp")
            staged.write_bytes(image)
            staged.replace(args.output)
            return inspect_image(args.output)
        if args.toolchain_command == "verify-package":
            from symbian.packaging.verification import verify_package

            return verify_package(
                args.package, args.executable, args.oracles_build, args.output
            )
        if args.toolchain_command == "verify-pointers":
            from symbian.toolchain.verification import verify_pointers

            return verify_pointers(
                args.artifact, args.oracles_build, args.output, args.package
            )
        from symbian.toolchain.verification import verify_probe

        return verify_probe(args.artifact, args.oracles_build, args.output)
    if args.command == "emu":
        if args.emu_command in ("resolve", "configure"):
            from symbian.emulator.configuration import configure, resolve
            from symbian.emulator.firmware import describe

            if args.emu_command == "configure":
                import os

                from symbian.emulator.configuration import config_path
                from symbian.emulator.firmware import selected

                path = config_path(
                    args.scope, project=args.project, sdk=args.sdk
                )
                values = {
                    key: (
                        (
                            str(value)
                            if value.is_absolute()
                            else os.path.relpath(value.resolve(), path.parent)
                        )
                        if isinstance(value, Path)
                        else value
                    )
                    for key, value in options(args).items()
                }
                if args.scope != "global" and args.firmware:
                    resolved = resolve(
                        project=args.project,
                        sdk=args.sdk,
                        overrides=options(args),
                        root=args.root.resolve(),
                    )
                    _, manifest = selected(resolved)
                    values["firmware"] = f"sha256:{manifest.identity}"
                if args.clear_firmware:
                    values["firmware"] = None
                return configure(
                    args.scope,
                    values,
                    unset=args.unset,
                    project=args.project,
                    sdk=args.sdk,
                )
            return describe(
                resolve(
                    project=args.project,
                    sdk=args.sdk,
                    overrides=options(args),
                    root=args.root.resolve(),
                )
            )
        if args.emu_command == "configure-ide":
            from symbian.emulator.ide import configure

            return configure(args.root, args.gdb)
        from symbian.emulator import Control

        control = Control(args.endpoint, timeout=args.timeout)
        if args.emu_command == "screenshot":
            return control.capture(args.name)
        if args.emu_command == "pointer":
            return control.pointer(args.x, args.y, args.action)
        return control.exit_report() if args.saved else control.status()
    if args.command == "build":
        if (args.project / "sdk-location.json").is_file():
            import os

            from symbian.project.configuration import ProjectConfiguration

            sdk = ProjectConfiguration.load(args.project.resolve()).sdk
            if os.environ.get("SYMBIAN_ACTIVE_SDK") != str(sdk.prefix):
                os.execv(
                    str(sdk.prefix / "bin/symbian"),
                    [
                        str(sdk.prefix / "bin/symbian"),
                        "build",
                        "--project",
                        str(args.project.resolve()),
                        "--output",
                        str(args.output.resolve()),
                        "--output-format",
                        args.output_format,
                        *(
                            ["--architecture", args.architecture]
                            if args.architecture
                            else []
                        ),
                    ],
                )
            args.compiler, args.linker = str(sdk.compiler), str(sdk.linker)
        return toolchain.build(
            args.project,
            args.output,
            args.compiler,
            args.linker,
            architecture=args.architecture,
        )
    if args.command == "package":
        if (args.project / "sdk-location.json").is_file():
            import os

            from symbian.project.configuration import ProjectConfiguration

            sdk = ProjectConfiguration.load(args.project.resolve()).sdk
            if os.environ.get("SYMBIAN_ACTIVE_SDK") != str(sdk.prefix):
                os.execv(
                    str(sdk.prefix / "bin/symbian"),
                    [
                        str(sdk.prefix / "bin/symbian"),
                        "package",
                        "--project",
                        str(args.project.resolve()),
                        "--artifact",
                        str(args.artifact.resolve()),
                        "--output",
                        str(args.output.resolve()),
                        "--output-format",
                        args.output_format,
                    ],
                )
        return packaging.package(args.project, args.artifact, args.output)
    if args.command == "inspect":
        from symbian.sdk import inspect_proxy

        inspector = {
            "elf32": inspect_elf,
            "e32": inspect_image,
            "sis": packaging.inspect_package,
            "import-proxy": inspect_proxy,
        }[args.format]
        return {"format": args.format, "metadata": inspector(args.artifact)}
    if args.command == "preserve":
        if args.preserve_command == "create":
            return preservation.create(
                args.source, args.archive, args.provenance
            )
        return preservation.verify(args.archive, args.manifest_sha256)
    if args.device_command == "policy":
        return device.policy(args.operation)
    from symbian.device.connection import inspect_device, list_devices

    if args.device_command == "list":
        return list_devices()
    if args.device_command == "info":
        if args.at_status and args.no_protocol:
            from symbian.status import Code, StatusError

            raise StatusError(
                Code.INVALID_ARGUMENT,
                "--at-status requires the protocol probe",
            )
        if args.mtp_list < 0 or args.mtp_list > 128:
            from symbian.status import Code, StatusError

            raise StatusError(
                Code.INVALID_ARGUMENT, "--mtp-list must be 0..128"
            )
        return inspect_device(
            args.device,
            probe_protocol=not args.no_protocol,
            usb_map=args.usb_map,
            at_status=args.at_status,
            mtp=args.mtp,
            mtp_list=args.mtp_list,
            obex_connect=args.obex_connect,
        )
    if args.device_command == "mode":
        from symbian.device.mode import begin, verify

        if args.mode_command == "begin":
            return begin(args.ticket, args.device)
        return verify(args.ticket)
    from symbian.device.installation import install

    return install(
        args.project,
        args.device,
        args.volume,
        package_path=args.package,
        compiler=args.compiler,
        linker=args.linker,
    )


def main(argv: list[str] | None = None) -> int:
    """Emits human or canonical JSON output and returns a process exit code."""
    parser = _parser()
    arguments = sys.argv[1:] if argv is None else argv
    if not arguments:
        parser.print_help()
        return 0
    args = parser.parse_args(arguments)
    if args.command == "console":
        try:
            from symbian.console.launcher import launch_detached
            from symbian.status import StatusException

            launch_detached(args.workdir)
            return 0
        except (OSError, ImportError, StatusException) as error:
            print(f"Console unavailable: {error}", file=sys.stderr)
            return 1
    if args.command == "doctor":
        # Diagnostics must still work before the native wheel is installed.
        response = {
            "schema": "symbian.cli/v1",
            "status": {"code": 0, "name": "OK", "message": ""},
            "result": doctor(),
        }
        print(render(response, args.output_format, args.command))
        return 0
    from symbian.status import Code, StatusError

    try:
        result = _execute(args)
        response = {
            "schema": "symbian.cli/v1",
            "status": {"code": 0, "name": "OK", "message": ""},
            "result": result,
        }
        exit_code = 0
    except (StatusError, OSError) as error:
        if isinstance(error, OSError):
            import ssl

            if isinstance(error, ssl.SSLCertVerificationError):
                code = Code.UNAUTHENTICATED
            elif isinstance(error, TimeoutError):
                code = Code.DEADLINE_EXCEEDED
            elif isinstance(error, ConnectionRefusedError):
                code = Code.UNAVAILABLE
            elif isinstance(error, FileNotFoundError):
                code = Code.NOT_FOUND
            elif isinstance(error, PermissionError):
                code = Code.PERMISSION_DENIED
            else:
                code = Code.INTERNAL
            error = StatusError(code, str(error))
        response = {"schema": "symbian.cli/v1", "status": error.as_dict()}
        exit_code = 1
    action = (
        getattr(args, f"{args.command}_command", "")
        if args.command in ("device", "firmware", "emu", "app", "sdk", "agent")
        else ""
    )
    formatted = render(response, args.output_format, args.command, action)
    print(
        formatted,
        file=(
            sys.stderr
            if exit_code and args.output_format == "human"
            else sys.stdout
        ),
    )
    return exit_code


if __name__ == "__main__":
    sys.exit(main())
