"""Independent image, CPU and emulator process checks for the E32 probe."""

import hashlib
import json
import os
import tempfile
from pathlib import Path

from symbian.e32 import inspect_image
from symbian.process import run
from symbian.status import Code, StatusError

ORACLES = (
    ("symbian_e32_oracle", 4),
    ("symbian_checksum_oracle", 1),
    ("symbian_validator_oracle", 7),
    ("symbian_cpu_probe", 8),
    ("symbian_process_probe", 8),
)


def _check_results(path: Path, expected: int) -> dict:
    try:
        results = json.loads(path.read_text(encoding="utf-8"))
    except (OSError, UnicodeError, json.JSONDecodeError) as error:
        raise StatusError(
            Code.DATA_LOSS, f"Missing/invalid native test results: {path}"
        ) from error
    if not isinstance(results, dict):
        raise StatusError(
            Code.DATA_LOSS, "Native test report must be an object"
        )
    cases = []
    suites = results.get("testsuites")
    if not isinstance(suites, list):
        raise StatusError(Code.DATA_LOSS, "Native test suites are missing")
    for suite in suites:
        if not isinstance(suite, dict) or not isinstance(
            suite.get("testsuite"), list
        ):
            raise StatusError(Code.DATA_LOSS, "Malformed native test suite")
        cases.extend(suite["testsuite"])
    if (
        results.get("tests") != expected
        or any(
            results.get(key) != 0 for key in ("failures", "disabled", "errors")
        )
        or len(cases) != expected
        or any(
            not isinstance(case, dict)
            or case.get("status") != "RUN"
            or case.get("result") != "COMPLETED"
            or case.get("failures")
            for case in cases
        )
    ):
        raise StatusError(
            Code.FAILED_PRECONDITION,
            f"Expected {expected} completed passing native tests: {path}",
        )
    return {"tests": expected, "results": str(path), "passed": True}


def run_oracles(
    fixtures: dict[str, Path],
    oracles: tuple[tuple[str, int], ...],
    oracles_build: Path,
    output: Path,
) -> tuple[list[dict], dict[str, Path]]:
    """Runs complete native suites against retained private fixture copies."""
    executables = [
        oracles_build / "platform-tests" / name for name, _ in oracles
    ]
    for executable in executables:
        if not executable.is_file():
            raise StatusError(
                Code.NOT_FOUND, f"Build native oracle: {executable}"
            )
    output.mkdir(parents=True, exist_ok=True)
    directory = Path(tempfile.mkdtemp(prefix="run-", dir=output))
    originals = {key: path.read_bytes() for key, path in fixtures.items()}
    copies = {}
    for key, path in fixtures.items():
        copy = directory / path.name
        if copy in copies.values():
            raise StatusError(
                Code.INVALID_ARGUMENT, "Duplicate oracle fixture basename"
            )
        copy.write_bytes(originals[key])
        copies[key] = copy
    # A private input copy ties every oracle to exactly the reported bytes.
    # Inherited test filters/sharding must never turn partial execution green.
    env = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("GTEST_", "SYMBIAN_"))
    }
    env.update({key: str(path) for key, path in copies.items()})
    checks = []
    try:
        for executable, (_, count) in zip(executables, oracles, strict=True):
            results_path = directory / f"{executable.name}.json"
            binary_digest = hashlib.sha256(executable.read_bytes()).hexdigest()
            stdout = run(
                [
                    str(executable),
                    "--gtest_filter=*",
                    "--gtest_repeat=1",
                    f"--gtest_output=json:{results_path}",
                ],
                cwd=directory,
                env=env,
                timeout=15,
            )
            log = directory / f"{executable.name}.log"
            log.write_text(stdout + "\n", encoding="utf-8")
            if (
                hashlib.sha256(executable.read_bytes()).hexdigest()
                != binary_digest
            ):
                raise StatusError(
                    Code.ABORTED, f"Oracle binary changed: {executable}"
                )
            checks.append(
                {
                    "name": executable.name,
                    "binary": str(executable),
                    "binary_sha256": binary_digest,
                    "log": str(log),
                    **_check_results(results_path, count),
                }
            )
    except StatusError as error:
        raise StatusError(
            error.code, f"{error.message}; verification artifacts: {directory}"
        ) from error
    if any(
        path.read_bytes() != originals[key]
        or copies[key].read_bytes() != originals[key]
        for key, path in fixtures.items()
    ):
        raise StatusError(
            Code.ABORTED, "Oracle fixture changed during verification"
        )
    return checks, copies


def verify_probe(artifact: Path, oracles_build: Path, output: Path) -> dict:
    """Checks a maintained integer probe, preserving native test evidence.

    Args:
        artifact: Converted e32_probe or cxx20_module_probe executable.
        oracles_build: EKA2L1 CMake build containing platform-tests executables.
        output: Directory for independent test JSON, logs and the final report.

    Returns:
        Verification evidence with historical validation, CPU execution and
        ROMless emulator process results. Belle runtime verification is false.
    """
    artifact = artifact.resolve()
    oracles_build = oracles_build.resolve()
    output = output.resolve()
    data = artifact.read_bytes()
    metadata = inspect_image(artifact)
    descriptor = metadata["exception_descriptor_offset"]
    expected_relocations = (
        [descriptor + offset for offset in (0, 4, 8, 12)]
        if descriptor
        else []
    )
    if (
        metadata["uid3"] != 0xE0000808
        or metadata["entry_offset"] != 0
        or metadata["dll"]
        or metadata["imports"]
        or metadata["code_relocations"] != expected_relocations
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Oracles require a maintained integer probe"
        )
    checks, copies = run_oracles(
        {"SYMBIAN_E32_TEST_IMAGE": artifact}, ORACLES, oracles_build, output
    )
    fixture = copies["SYMBIAN_E32_TEST_IMAGE"]
    if artifact.read_bytes() != data or fixture.read_bytes() != data:
        raise StatusError(
            Code.ABORTED, "Probe bytes changed during verification"
        )
    report = {
        "schema": "symbian.e32-probe-verification/v1",
        "artifact": str(artifact),
        "sha256": hashlib.sha256(data).hexdigest(),
        "tested_copy": str(fixture),
        "e32": metadata,
        "oracles": checks,
        "tests_passed": sum(count for _, count in ORACLES),
        "historical_image_validation_passed": True,
        "cpu_probe_verified": True,
        "cpu_backends": ["dyncom", "dynarmic"],
        "code_load_addresses": [0x8000, 0x20000],
        "exit_svc_observed": True,
        "eka2l1_process_verified": True,
        "kernel_exit_verified": True,
        "emulator_os_profile": "epoc10",
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "limitations": [
            "Checks the maintained integer probe only",
            "Historical pre-Belle validator with host type adapters",
            "CPU-only cases observe SVC registers without kernel dispatch",
            "Process cases use EKA2L1's host kernel and epoc10 SVC table",
            "No Belle ROM/Z, target DLLs, services or full C++ ABI test",
        ],
    }
    report_path = output / "report.json"
    report["report"] = str(report_path)
    report_path.write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report


POINTER_ORACLES = (
    ("symbian_checksum_oracle", 1),
    ("symbian_validator_oracle", 7),
    ("symbian_pointer_probe", 6),
)


def verify_gui(artifact: Path, oracles_build: Path, output: Path) -> dict:
    """Validates the maintained GUI image without executing system imports.

    Args:
        artifact: Generated examples/gui_app E32 executable.
        oracles_build: Research build containing the two historical oracles.
        output: Directory retaining private fixtures, logs and a report.

    Returns:
        Eight historical checksum/validation results. GUI execution, matched
        DLLs, Belle loader compatibility and debugger attachment remain false.
    """
    artifact, oracles_build, output = (
        path.resolve() for path in (artifact, oracles_build, output)
    )
    if artifact.is_relative_to(output):
        raise StatusError(Code.INVALID_ARGUMENT, "Keep image outside checks")
    data = artifact.read_bytes()
    metadata = inspect_image(artifact)
    selections = {
        item["dll"]: len(item["slots"]) for item in metadata["imports"]
    }
    if (
        metadata["uid3"] != 0xE0000811
        or metadata["dll"]
        or metadata["entry_offset"] != 0
        or not {"euser.dll", "ws32.dll"}.issubset(selections)
        or not set(selections).issubset(
            {
                "euser.dll",
                "ws32.dll",
                "gdi.dll",
                "libc.dll",
                "libm.dll",
                "libpthread.dll",
                "drtaeabi.dll",
            }
        )
        or any(count == 0 for count in selections.values())
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Expected the maintained GUI image profile"
        )
    oracles = POINTER_ORACLES[:2]
    checks, copies = run_oracles(
        {"SYMBIAN_E32_TEST_IMAGE": artifact}, oracles, oracles_build, output
    )
    if artifact.read_bytes() != data:
        raise StatusError(Code.ABORTED, "GUI image changed during checks")
    report = {
        "schema": "symbian.gui-image-validation/v1",
        "artifact": str(artifact),
        "sha256": hashlib.sha256(data).hexdigest(),
        "tested_copy": str(copies["SYMBIAN_E32_TEST_IMAGE"]),
        "e32": metadata,
        "oracles": checks,
        "tests_passed": sum(count for _, count in oracles),
        "historical_image_validation_passed": True,
        "gui_execution_verified": False,
        "debugger_attachment_verified": False,
        "import_execution_verified": False,
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "limitations": [
            "Historical pre-Belle source validation with host type adapters",
            "Does not launch this image or supply EUSER/WS32 implementations",
            "Requires matched ROM/Z and a running Window Server for GUI tests",
        ],
    }
    path = output / "report.json"
    report["report"] = str(path)
    path.write_text(json.dumps(report, indent=2) + "\n", encoding="utf-8")
    return report


def verify_pointers(
    artifact: Path,
    oracles_build: Path,
    output: Path,
    package: Path | None = None,
) -> dict:
    """Checks the maintained pointer probe with the original loader consumers.

    Args:
        artifact: Converted pointer_probe or cxx20_probe executable.
        oracles_build: Research build with the independent GTest consumers.
        output: Directory preserving test JSON, logs, copies and final evidence.
        package: Optional canonical SISX wrapping this exact maintained image.

    Returns:
        Scoped ROMless pointer/virtual dispatch and optional installer evidence.
        Matched Belle and physical execution remain unverified.
    """
    artifact, oracles_build, output = (
        path.resolve() for path in (artifact, oracles_build, output)
    )
    inputs = {artifact}
    if package is not None:
        package = package.resolve()
        inputs.add(package)
    generated = [output / "report.json"]
    if package is not None:
        generated.append(output / "expected-image.sha1")
    if any(path.resolve() in inputs for path in generated):
        raise StatusError(
            Code.INVALID_ARGUMENT, "Verification would overwrite input"
        )
    data = artifact.read_bytes()
    metadata = inspect_image(artifact)
    if (
        metadata["uid3"] != 0xE0000808
        or metadata["entry_offset"] != 0
        or metadata["dll"]
        or metadata["imports"]
        or len(metadata["code_relocations"]) != 4
    ):
        raise StatusError(
            Code.INVALID_ARGUMENT,
            "Oracles require a maintained callback/virtual probe",
        )
    checks, copies = run_oracles(
        {"SYMBIAN_E32_TEST_IMAGE": artifact},
        POINTER_ORACLES,
        oracles_build,
        output,
    )
    report = {
        "schema": "symbian.pointer-probe-verification/v1",
        "artifact": str(artifact),
        "sha256": hashlib.sha256(data).hexdigest(),
        "tested_copy": str(copies["SYMBIAN_E32_TEST_IMAGE"]),
        "e32": metadata,
        "oracles": checks,
        "tests_passed": sum(count for _, count in POINTER_ORACLES),
        "historical_image_validation_passed": True,
        "mapped_pointer_words_verified": True,
        "indirect_calls_verified": True,
        "virtual_dispatch_verified": True,
        "eka2l1_process_verified": True,
        "kernel_exit_verified": True,
        "cpu_backends": ["dyncom", "dynarmic"],
        "emulator_os_profile": "epoc10",
        "symbian_loader_verified": False,
        "runtime_verified": False,
        "eka2l1_install_launch_verified": False,
        "physical_installation_verified": False,
        "limitations": [
            "Maintained ARM/Thumb callback and virtual method probe",
            "Single inheritance without RTTI/exceptions; no full C++ ABI test",
            "No writable data/TLS/static lifetime or SDK startup",
            "ROMless epoc10 host kernel; no matched Belle ROM/Z or system DLLs",
        ],
    }
    if package is not None:
        from symbian.packaging import inspect_package

        package = package.resolve()
        package_bytes = package.read_bytes()
        package_metadata = inspect_package(package)
        if any(
            package_metadata[field] != expected
            for field, expected in {
                "uid": 0xE0000809,
                "name": "Symbian E32 Probe",
                "vendor": "Symbian research",
                "executable_name": "probe.exe",
                "version": [1, 0, 0],
                "executable_uid": 0xE0000808,
                "executable_size": len(data),
            }.items()
        ):
            raise StatusError(
                Code.INVALID_ARGUMENT, "Unexpected pointer package"
            )
        output.mkdir(parents=True, exist_ok=True)
        reference = output / "expected-image.sha1"
        reference.write_bytes(hashlib.sha1(data).digest())
        package_checks, package_copies = run_oracles(
            {
                "SYMBIAN_E32_TEST_IMAGE": artifact,
                "SYMBIAN_SIS_TEST_PACKAGE": package,
                "SYMBIAN_E32_TEST_HASH": reference,
            },
            (("symbian_sis_checksum_oracle", 1), ("symbian_package_probe", 6)),
            oracles_build,
            output / "package",
        )
        if package.read_bytes() != package_bytes:
            raise StatusError(
                Code.ABORTED, "Pointer package changed during checks"
            )
        report.update(
            {
                "package": str(package),
                "package_sha256": hashlib.sha256(package_bytes).hexdigest(),
                "tested_package": str(
                    package_copies["SYMBIAN_SIS_TEST_PACKAGE"]
                ),
                "package_oracles": package_checks,
                "expected_image_sha1": reference.read_bytes().hex(),
                "eka2l1_install_launch_verified": True,
                "registry_reload_verified": True,
                "uninstall_reinstall_verified": True,
                "tests_passed": report["tests_passed"] + 7,
            }
        )
    if artifact.read_bytes() != data:
        raise StatusError(Code.ABORTED, "Pointer probe changed during checks")
    report_path = output / "report.json"
    report["report"] = str(report_path)
    report_path.write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report
