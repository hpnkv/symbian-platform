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
        if not key.startswith("GTEST_")
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
    """Checks the maintained e32_probe, preserving native test evidence.

    Args:
        artifact: Converter output for the maintained e32_probe example.
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
    if metadata["uid3"] != 0xE0000808 or metadata["entry_offset"] != 0:
        raise StatusError(
            Code.INVALID_ARGUMENT, "Oracles require the maintained e32_probe"
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
