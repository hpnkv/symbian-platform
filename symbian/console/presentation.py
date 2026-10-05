"""Task-oriented presentation of the public SDK command catalog."""

from collections.abc import Mapping
from typing import Literal

from pydantic import Field

from symbian.console.models import (
    CommandArgument,
    CommandResult,
    CommandSpec,
    ConsoleModel,
    OutcomeFact,
    OutcomeSummary,
)


class TaskPresentation(ConsoleModel):
    """Human-facing guidance for one public SDK operation."""

    path: tuple[str, ...] = Field(description="Canonical CLI path")
    group: str = Field(description="Navigation group")
    title: str = Field(description="Task name shown in the console")
    summary: str = Field(description="Outcome and scope shown before entry")
    action: str = Field(default="Run", description="Final action button label")
    view_mode: Literal["workflow", "live"] = Field(
        default="workflow",
        description="Guided action or immediately refreshed read-only view",
    )
    review_required: bool = Field(
        default=False,
        description="Show a final confirmation for consequential changes",
    )


class TaskStep(ConsoleModel):
    """One wizard page collecting related inputs."""

    title: str = Field(description="Wizard step name")
    guidance: str = Field(description="Visible guidance for the step")
    arguments: tuple[CommandArgument, ...] = Field(
        default=(), description="Inputs collected on this page"
    )
    optional: bool = Field(
        default=False,
        description="Step may be skipped when current values suffice",
        exclude_if=lambda value: not value,
    )


# These labels describe user tasks, while the CLI parser remains authoritative
# for flags, choices, defaults and execution semantics.
_TASKS = (
    (
        "agent hello",
        "Devices",
        "Inspect agent handshake",
        "Authenticate the development agent and inspect its protocol greeting.",
    ),
    (
        "agent status",
        "Devices",
        "Read agent status",
        "Read bounded status from an authenticated development agent.",
    ),
    (
        "agent logs",
        "Devices",
        "Read recent agent logs",
        "Read a bounded page of development-agent service events.",
    ),
    (
        "agent files",
        "Devices",
        "List agent workspace",
        "List the authenticated agent's read-only development workspace.",
    ),
    (
        "agent listen",
        "Devices",
        "Listen for agent connection",
        "Accept a phone-initiated authenticated status or log session.",
    ),
    (
        "doctor",
        "Getting started",
        "Check this computer",
        "See which SDK tools and capabilities are ready.",
    ),
    (
        "init",
        "Applications",
        "Create an application",
        "Choose its folder, identity, target and initial build.",
    ),
    (
        "app build",
        "Applications",
        "Build an existing application",
        "Build and verify a standalone application project.",
    ),
    (
        "app run",
        "Applications",
        "Run an application",
        "Build and launch an application in an emulator.",
    ),
    (
        "app configure",
        "Applications",
        "Update project settings",
        "Regenerate SDK and IDE settings for an application.",
    ),
    (
        "build",
        "Applications",
        "Build a source project",
        "Produce an ARM/E32 executable from a project.",
    ),
    (
        "package",
        "Applications",
        "Package an application",
        "Create a SIS package from an executable and resources.",
    ),
    (
        "firmware import",
        "Firmware",
        "Import firmware",
        "Select a ROM, archive, bundle or existing emulator instance.",
    ),
    (
        "firmware list",
        "Firmware",
        "Browse imported firmware",
        "List identities already held in the content store.",
    ),
    (
        "firmware inspect",
        "Firmware",
        "Inspect imported firmware",
        "Read the profile of one imported identity.",
    ),
    (
        "firmware export",
        "Firmware",
        "Export firmware bundle",
        "Create a portable copy of an imported identity.",
    ),
    (
        "firmware probe",
        "Firmware",
        "Examine a firmware source",
        "Read an archive or source without importing it.",
    ),
    (
        "emu run",
        "Emulator",
        "Run emulator",
        "Open a fresh emulator session without launching an application. "
        "The selected application is built and staged for manual launch. "
        "Close the emulator window to finish the session.",
    ),
    (
        "signing list",
        "Signing",
        "Browse signing identities",
        "View local identities, certificate fingerprints and expiry dates.",
    ),
    (
        "signing create",
        "Signing",
        "Create signing identity",
        "Create a private RSA key and self-signed certificate.",
    ),
    (
        "signing import",
        "Signing",
        "Import signing identity",
        "Copy a matching PEM certificate and unencrypted RSA key "
        "into private storage.",
    ),
    (
        "signing archive",
        "Signing",
        "Archive signing identity",
        "Remove an identity from active use and retain its keys "
        "in the archive.",
    ),
    (
        "signing sign",
        "Signing",
        "Sign an application",
        "Sign an existing SIS with a local identity and save a new SIS file. "
        "Self-signing does not establish trust on a phone.",
    ),
    (
        "emu resolve",
        "Emulator",
        "Review emulator settings",
        "See the effective project, SDK and global settings.",
    ),
    (
        "emu configure",
        "Emulator",
        "Save emulator settings",
        "Choose a configuration level and the values to save.",
    ),
    (
        "emu configure-ide",
        "Emulator",
        "Set up IDE debugging",
        "Generate the emulator's IDE debug integration.",
    ),
    (
        "emu status",
        "Emulator",
        "Inspect an emulator session",
        "Read live or retained session status.",
    ),
    (
        "emu screenshot",
        "Emulator",
        "Save a screenshot",
        "Capture the selected emulator display.",
    ),
    (
        "emu pointer",
        "Emulator",
        "Send pointer input",
        "Send a pointer transition to an owned emulator.",
    ),
    (
        "inspect",
        "Inspection",
        "Inspect a binary or package",
        "Read ELF, E32, SIS or import-proxy metadata.",
    ),
    (
        "toolchain probe",
        "Inspection",
        "Check the ARM toolchain",
        "Compile a small reproducible ARM object.",
    ),
    (
        "toolchain verify-probe",
        "Inspection",
        "Verify an executable",
        "Check an E32 artifact against independent oracles.",
    ),
    (
        "toolchain verify-pointers",
        "Inspection",
        "Verify pointer input",
        "Check an artifact's pointer behavior.",
    ),
    (
        "toolchain verify-package",
        "Inspection",
        "Verify a package",
        "Check a SIS package and its executable together.",
    ),
    (
        "toolchain verify-gui",
        "Inspection",
        "Verify GUI behavior",
        "Run guarded emulator GUI checks for an executable.",
    ),
    (
        "toolchain verify-gui-package",
        "Inspection",
        "Verify a GUI package",
        "Check package installation and GUI behavior.",
    ),
    (
        "prepare-app-sdk",
        "SDK and toolchain",
        "Prepare an application SDK",
        "Prepare source inputs for a new SDK export.",
    ),
    (
        "sdk install",
        "SDK and toolchain",
        "Install an SDK",
        "Copy an existing SDK or export one from prepared sources.",
    ),
    (
        "toolchain import-proxy",
        "SDK and toolchain",
        "Build an import proxy",
        "Select frozen exports for a target DLL.",
    ),
    (
        "toolchain convert-dll",
        "SDK and toolchain",
        "Convert a linked DLL",
        "Convert an ARM ELF with a frozen export definition.",
    ),
    (
        "toolchain prepare-gui-sdk",
        "SDK and toolchain",
        "Prepare a GUI SDK",
        "Build a GUI-capable SDK from pinned source inputs.",
    ),
    (
        "preserve create",
        "Preservation",
        "Preserve a source",
        "Create a verifiable host archive with provenance.",
    ),
    (
        "preserve verify",
        "Preservation",
        "Verify an archive",
        "Check a preserved archive against its manifest.",
    ),
    (
        "device list",
        "Devices",
        "Find connected phones",
        "Show supported Symbian USB candidates.",
    ),
    (
        "device info",
        "Devices",
        "Inspect a phone",
        "Read USB, storage and optional protocol evidence.",
    ),
    (
        "device mode begin",
        "Devices",
        "Record a USB mode baseline",
        "Save a private baseline before switching the phone's USB mode.",
    ),
    (
        "device mode verify",
        "Devices",
        "Verify a USB mode change",
        "Compare a connected phone against a saved baseline.",
    ),
    (
        "device install",
        "Devices",
        "Stage an application on a phone",
        "Copy a SIS for a human-approved on-phone install.",
    ),
    (
        "device policy",
        "Devices",
        "Review device policy",
        "Check whether a hardware operation has an executor.",
    ),
)

_ACTIONS = {
    "doctor": "Check computer",
    "init": "Create application",
    "app build": "Build application",
    "app run": "Run application",
    "app configure": "Update settings",
    "build": "Build executable",
    "package": "Create package",
    "firmware import": "Import firmware",
    "firmware export": "Export bundle",
    "emu configure": "Save settings",
    "emu run": "Run emulator",
    "signing list": "Refresh identities",
    "signing create": "Create identity",
    "signing import": "Import identity",
    "signing archive": "Archive identity",
    "signing sign": "Sign application",
    "emu screenshot": "Capture screenshot",
    "emu pointer": "Send pointer action",
    "inspect": "Inspect file",
    "device info": "Inspect phone",
    "preserve create": "Preserve source",
    "preserve verify": "Verify archive",
    "device mode begin": "Save baseline",
    "device mode verify": "Verify mode change",
    "device install": "Stage package",
}

PRESENTATIONS = {
    tuple(command.split()): TaskPresentation(
        path=tuple(command.split()),
        group=group,
        title=title,
        summary=summary,
        action=_ACTIONS.get(command, "Run task"),
        view_mode=(
            "live"
            if tuple(command.split())
            in {
                ("doctor",),
                ("firmware", "list"),
                ("emu", "resolve"),
                ("signing", "list"),
                ("device", "list"),
            }
            else "workflow"
        ),
        review_required=tuple(command.split())
        in {
            ("init",),
            ("firmware", "import"),
            ("emu", "configure"),
            ("sdk", "install"),
            ("preserve", "create"),
            ("device", "mode", "begin"),
            ("device", "install"),
        },
    )
    for command, group, title, summary in _TASKS
}

GROUPS = (
    "Getting started",
    "Applications",
    "Firmware",
    "Emulator",
    "Signing",
    "Inspection",
    "SDK and toolchain",
    "Preservation",
    "Devices",
)

LABELS = {
    "destination": "Destination",
    "identities": "Signing identity folder",
    "identity": "Signing identity name",
    "common_name": "Certificate display name",
    "certificate": "PEM certificate file",
    "private_key": "PEM private key file",
    "sdk": "Installed SDK",
    "name": "Name",
    "ide": "IDE integration",
    "architecture": "Target architecture",
    "uid3": "Application UID3",
    "non_interactive": "Use supplied defaults without prompts",
    "portable_runtime": "Portable runtime",
    "firmware": "Firmware selection",
    "store": "Shared content store",
    "emulator": "Emulator executable",
    "importer": "Firmware importer",
    "backend": "CPU backend",
    "language": "System language number",
    "profile": "Compatibility profile",
    "no_build": "Skip initial build",
    "project": "Application folder",
    "workspace": "Prepared source workspace",
    "output": "Output folder",
    "compiler": "Clang C++ executable",
    "linker": "LLD linker executable",
    "artifact": "Executable or package file",
    "package": "SIS package file",
    "executable": "Associated E32 executable",
    "oracles_build": "Built emulator oracle tools",
    "definition": "Frozen DEF file",
    "symbol": "Exported symbol",
    "target_dll": "Target DLL name",
    "headers": "Platform headers folder",
    "elf": "Linked ARM ELF file",
    "import_proxy": "Import proxy library",
    "sources_root": "Pinned source checkout",
    "root": "Workspace root",
    "scope": "Settings level",
    "unset": "Setting to remove",
    "clear_firmware": "Clear saved firmware",
    "gdb": "ARM GDB executable",
    "endpoint": "Emulator control socket",
    "timeout": "Timeout in seconds",
    "saved": "Read retained exit report",
    "x": "Horizontal position (pixels)",
    "y": "Vertical position (pixels)",
    "action": "Pointer action",
    "source": "Source file or folder",
    "rom": "Unpacked ROM image",
    "vpl": "VPL manifest",
    "instance": "Existing emulator instance",
    "bundle": "Portable firmware bundle",
    "rpkg": "RPKG companion",
    "z_drive": "Drive-Z companion",
    "variant": "ROM variant index",
    "replace_alias": "Replace existing alias",
    "use": "Save imported firmware as default",
    "reference": "Firmware alias or content ID",
    "format": "Artifact format",
    "archive": "Preserved archive folder",
    "provenance": "Source provenance",
    "manifest_sha256": "Expected manifest digest",
    "operation": "Device operation",
    "device": "Connected phone",
    "no_protocol": "Skip AT modem query",
    "usb_map": "Show USB map",
    "at_status": "Read AT battery and signal codes",
    "mtp": "Read MTP metadata",
    "mtp_list": "MTP root item limit",
    "obex_connect": "Try PC Suite OBEX connection",
    "ticket": "Private mode ticket file",
    "volume": "Destination volume",
}


def step_name(path: tuple[str, ...], argument: CommandArgument) -> str:
    """Place an input on a task-specific wizard page."""
    name = argument.name
    if path == ("init",):
        if name in {"destination", "name", "uid3"}:
            return "Application"
        if name in {
            "sdk",
            "ide",
            "architecture",
            "portable_runtime",
            "non_interactive",
            "no_build",
        }:
            return "SDK and build"
        return "Emulator"
    if path == ("firmware", "import"):
        if name in {
            "source",
            "rom",
            "vpl",
            "instance",
            "bundle",
            "rpkg",
            "z_drive",
        }:
            return "Source"
        if name in {"variant", "name", "replace_alias", "use"}:
            return "Identity"
        return "Environment"
    if path == ("emu", "configure"):
        return (
            "Where to save"
            if name in {"scope", "project", "sdk", "root"}
            else "Settings"
        )
    if path == ("device", "install"):
        return (
            "Application"
            if name in {"project", "package", "compiler", "linker"}
            else "Phone"
        )
    if path == ("device", "info"):
        return "Phone" if name == "device" else "Optional probes"
    if len(path) > 0 and len(path) > 1 and path[0] == "toolchain":
        return "Inputs" if argument.required else "Verification options"
    return "Inputs" if argument.required or argument.positional else "Options"


def task_steps(specification: CommandSpec) -> tuple[TaskStep, ...]:
    """Build concise wizard steps for a catalogued SDK task."""
    if not specification.arguments:
        return (
            TaskStep(
                title="Ready", guidance="No additional information is needed."
            ),
        )
    grouped: dict[str, list[CommandArgument]] = {}
    for argument in specification.arguments:
        grouped.setdefault(step_name(specification.path, argument), []).append(
            argument
        )
    preferred_order = {
        ("firmware", "import"): ("Source", "Identity", "Environment"),
        ("init",): ("Application", "SDK and build", "Emulator"),
        ("emu", "configure"): ("Where to save", "Settings"),
        ("device", "install"): ("Application", "Phone"),
    }.get(specification.path)
    if preferred_order is not None:
        grouped = {
            name: grouped[name] for name in preferred_order if name in grouped
        }
    else:
        grouped = dict(
            sorted(
                grouped.items(),
                key=lambda item: not any(
                    argument.required for argument in item[1]
                ),
            )
        )
    has_required_group = any(
        any(argument.required for argument in arguments)
        for arguments in grouped.values()
    )
    return tuple(
        TaskStep(
            title=name,
            guidance=(
                "Fields marked Required must be completed before continuing. "
                "Optional values use the SDK defaults when left empty."
            ),
            arguments=tuple(arguments),
            optional=(
                name
                in {
                    ("init",): {"SDK and build", "Emulator"},
                    ("firmware", "import"): {"Identity", "Environment"},
                }.get(specification.path, set())
                or (
                    has_required_group
                    and not any(argument.required for argument in arguments)
                )
            ),
        )
        for name, arguments in grouped.items()
    )


def summarize_command(result: CommandResult) -> OutcomeSummary:
    """Show key results first and leave the complete payload available."""
    task = PRESENTATIONS.get(
        result.path,
        TaskPresentation(
            path=result.path,
            group="SDK and toolchain",
            title=" ".join(result.path).title(),
            summary="SDK task result",
        ),
    )
    payload = result.result
    if not isinstance(payload, Mapping):
        return OutcomeSummary(
            title=f"{task.title} completed",
            message="The task finished successfully.",
        )
    facts: list[OutcomeFact] = []
    for key, label in (
        ("project", "Application folder"),
        ("artifact", "Artifact"),
        ("executable", "Executable"),
        ("package", "Package"),
        ("archive", "Archive"),
        ("destination", "Saved to"),
        ("firmware", "Firmware identity"),
        ("output", "Output folder"),
        ("name", "Identity"),
        ("certificate", "Certificate"),
    ):
        value = payload.get(key)
        if isinstance(value, (str, int)) and value:
            facts.append(OutcomeFact(label=label, value=str(value)))
        if len(facts) == 3:
            break
    if result.path == ("doctor",):
        tools = payload.get("tools")
        if isinstance(tools, Mapping):
            available = sum(bool(path) for path in tools.values())
            facts.append(
                OutcomeFact(
                    label="Tools found", value=f"{available} of {len(tools)}"
                )
            )
        native = payload.get("native_analysis")
        if isinstance(native, bool):
            facts.append(
                OutcomeFact(
                    label="Native inspection",
                    value="Available" if native else "Unavailable",
                )
            )
    if result.path == ("device", "list"):
        devices = payload.get("devices")
        if isinstance(devices, list):
            facts.append(
                OutcomeFact(label="Phones found", value=str(len(devices)))
            )
    message = "The task finished successfully."
    if result.path == ("init",):
        message = "Open the application folder to edit or build the project."
    elif result.path == ("device", "install"):
        message = "Complete installation on the phone when prompted."
    return OutcomeSummary(
        title=f"{task.title} completed",
        message=message,
        facts=tuple(facts),
    )
