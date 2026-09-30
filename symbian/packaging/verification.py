"""Independent SIS installation and execution for the maintained probe."""

import hashlib
import json
from pathlib import Path

from symbian.packaging import inspect_package
from symbian.status import Code, StatusError
from symbian.toolchain.verification import run_oracles, verify_gui, verify_probe

ORACLES = (("symbian_sis_checksum_oracle", 1), ("symbian_package_probe", 6))


def verify_package(
    package: Path, executable: Path, oracles_build: Path, output: Path
) -> dict:
    """Runs image, CPU, kernel, historical SIS CRC and installer checks.

    Args:
        package: Canonical e32_probe or cxx20_module_probe package.
        executable: Corresponding unchanged E32 output, used as an oracle.
        oracles_build: Native EKA2L1 build with the research GTest executables.
        output: Directory retaining input copies, logs and native test results.

    Returns:
        Scoped ROMless emulator evidence; Belle and phone flags remain false.
    """
    package, executable, oracles_build, output = (
        path.resolve() for path in (package, executable, oracles_build, output)
    )
    inputs = {package, executable}
    if any(
        path.resolve() in inputs
        for path in (output / "report.json", output / "expected-image.sha1")
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Verification would overwrite input"
        )
    metadata = inspect_package(package)
    data = package.read_bytes()
    image = executable.read_bytes()
    if (
        metadata["uid"] != 0xE0000809
        or metadata["executable_uid"] != 0xE0000808
        or metadata["name"] != "Symbian E32 Probe"
        or metadata["vendor"] != "Symbian research"
        or metadata["executable_name"] != "probe.exe"
        or metadata["version"] != [1, 0, 0]
        or metadata["executable_size"] != len(image)
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Oracles require the maintained probe package",
        )
    image_report = verify_probe(executable, oracles_build, output / "image")
    reference = output / "expected-image.sha1"
    reference.write_bytes(hashlib.sha1(image).digest())
    checks, copies = run_oracles(
        {
            "SYMBIAN_SIS_TEST_PACKAGE": package,
            "SYMBIAN_E32_TEST_IMAGE": executable,
            "SYMBIAN_E32_TEST_HASH": reference,
        },
        ORACLES,
        oracles_build,
        output,
    )
    if package.read_bytes() != data or executable.read_bytes() != image:
        raise StatusError(Code.ABORTED, "Package verification inputs changed")
    report = {
        "schema": "symbian.sis-probe-verification/v1",
        "artifact": str(package),
        "sha256": hashlib.sha256(data).hexdigest(),
        "executable": str(executable),
        "executable_sha256": hashlib.sha256(image).hexdigest(),
        "tested_copy": str(copies["SYMBIAN_SIS_TEST_PACKAGE"]),
        "sis": metadata,
        "expected_image_sha1": reference.read_bytes().hex(),
        "image_verification": image_report,
        "oracles": checks,
        "tests_passed": image_report["tests_passed"]
        + sum(n for _, n in ORACLES),
        "historical_sis_checksums_verified": True,
        "eka2l1_install_launch_verified": True,
        "registry_reload_verified": True,
        "uninstall_reinstall_verified": True,
        "cpu_backends": ["dyncom", "dynarmic"],
        "emulator_os_profile": "epoc10",
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "phone_installation_verified": False,
        "limitations": [
            "Unsigned import-free probe in disposable ROMless filesystems",
            "EKA2L1 installer does not enforce phone signing/capability policy",
            "No Belle ROM/Z, target DLLs, GUI or general application runtime",
        ],
    }
    path = output / "report.json"
    report["report"] = str(path)
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def verify_gui_package(
    package: Path, executable: Path, oracles_build: Path, output: Path
) -> dict:
    """Checks GUI installation and unresolved imports without booting an OS.

    Args:
        package: Maintained single-executable GUI package.
        executable: Exact E32 payload used to build the package.
        oracles_build: Research build containing image/SIS/GUI package oracles.
        output: Separate directory retaining inputs, logs and test reports.

    Returns:
        Historical image validation and ROMless installer evidence. GUI drawing,
        SDK startup, debugger attachment and matched Belle remain unverified.
    """
    package, executable, oracles_build, output = (
        path.resolve() for path in (package, executable, oracles_build, output)
    )
    if any(path.is_relative_to(output) for path in (package, executable)):
        raise StatusError(Code.INVALID_ARGUMENT, "Keep inputs outside checks")
    metadata = inspect_package(package)
    data, image = package.read_bytes(), executable.read_bytes()
    if (
        metadata["uid"] != 0xE0000812
        or metadata["executable_uid"] != 0xE0000811
        or metadata["name"] != "Symbian GUI Counter"
        or metadata["vendor"] != "Symbian research"
        or metadata["executable_name"] != "gui_app.exe"
        or metadata["version"] != [1, 0, 0]
        or metadata["executable_size"] != len(image)
        or metadata["executable_sha1"] != hashlib.sha1(image).hexdigest()
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Expected the exact maintained GUI package"
        )
    image_report = verify_gui(executable, oracles_build, output / "image")
    reference = output / "expected-image.sha1"
    reference.write_bytes(hashlib.sha1(image).digest())
    oracles = (
        ("symbian_sis_checksum_oracle", 1),
        ("symbian_gui_package_probe", 8),
    )
    checks, copies = run_oracles(
        {
            "SYMBIAN_SIS_TEST_PACKAGE": package,
            "SYMBIAN_E32_TEST_IMAGE": executable,
            "SYMBIAN_E32_TEST_HASH": reference,
        },
        oracles,
        oracles_build,
        output,
    )
    if package.read_bytes() != data or executable.read_bytes() != image:
        raise StatusError(Code.ABORTED, "GUI package inputs changed")
    report = {
        "schema": "symbian.gui-package-verification/v1",
        "artifact": str(package),
        "sha256": hashlib.sha256(data).hexdigest(),
        "executable": str(executable),
        "executable_sha256": hashlib.sha256(image).hexdigest(),
        "tested_copy": str(copies["SYMBIAN_SIS_TEST_PACKAGE"]),
        "sis": metadata,
        "image_verification": image_report,
        "oracles": checks,
        "tests_passed": image_report["tests_passed"] + 9,
        "historical_sis_checksums_verified": True,
        "eka2l1_install_verified": True,
        "registry_reload_verified": True,
        "uninstall_reinstall_verified": True,
        "missing_system_libraries_rejected": False,
        "unresolved_import_slots_observed": 37,
        "process_creation_without_system_libraries_observed": True,
        "cpu_backends_configured": ["dyncom", "dynarmic"],
        "cpu_instructions_executed": 0,
        "gui_execution_verified": False,
        "debugger_attachment_verified": False,
        "import_execution_verified": False,
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "phone_installation_verified": False,
        "limitations": [
            "Installer/registry tests in disposable ROMless C filesystems",
            "Upstream creates a process despite missing EUSER/WS32; "
            "all 37 import slots remain unresolved",
            "No booted OS, Window Server, visual output or guest debugging",
            "Emulator installer does not establish phone signing policy",
        ],
    }
    path = output / "report.json"
    report["report"] = str(path)
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report
