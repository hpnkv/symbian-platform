"""Independent verification must reject partial or missing native evidence."""

import json
import os
import shutil
from pathlib import Path

import pytest

from symbian import toolchain
from symbian.status import Code, StatusError
from symbian.tests.cli_json import main
from symbian.toolchain import verification


def _report(count: int) -> dict:
    return {
        "tests": count,
        "failures": 0,
        "disabled": 0,
        "errors": 0,
        "testsuites": [
            {
                "testsuite": [
                    {"status": "RUN", "result": "COMPLETED"}
                    for _ in range(count)
                ]
            }
        ],
    }


@pytest.fixture
def probe(tmp_path):
    if not shutil.which("clang++") or not shutil.which("ld.lld"):
        pytest.skip("Clang and LLD required")
    project = Path(__file__).parents[2] / "probes/e32_probe"
    result = toolchain.build(project, tmp_path / "build")
    return Path(result["artifact"])


@pytest.mark.parametrize("fault", ("filtered", "skipped", "failed", "disabled"))
def test_partial_or_failing_results_cannot_be_green(tmp_path, fault):
    report = _report(8)
    if fault == "filtered":
        report = _report(0)
    elif fault == "skipped":
        report["testsuites"][0]["testsuite"][0]["result"] = "SKIPPED"
    elif fault == "failed":
        report["failures"] = 1
    else:
        report["disabled"] = 1
    path = tmp_path / "native.json"
    path.write_text(json.dumps(report))
    with pytest.raises(StatusError) as caught:
        verification._check_results(path, 8)
    assert caught.value.code == Code.FAILED_PRECONDITION


def test_missing_native_json_has_status(tmp_path):
    with pytest.raises(StatusError) as caught:
        verification._check_results(tmp_path / "missing.json", 8)
    assert caught.value.code == Code.DATA_LOSS


def test_missing_oracle_is_not_fake_success(probe, tmp_path):
    with pytest.raises(StatusError) as caught:
        verification.verify_probe(probe, tmp_path / "missing", tmp_path / "out")
    assert caught.value.code == Code.NOT_FOUND


def test_isolated_input_and_inherited_test_filter(probe, tmp_path, monkeypatch):
    build = tmp_path / "oracles"
    executables = build / "platform-tests"
    executables.mkdir(parents=True)
    for name, _ in verification.ORACLES:
        (executables / name).write_bytes(b"test-double")
    monkeypatch.setenv("GTEST_FILTER", "nothing")
    monkeypatch.setenv("SYMBIAN_E32_TEST_HASH", "unrelated-reference")
    monkeypatch.setenv("GTEST_TOTAL_SHARDS", "99")
    monkeypatch.setenv("GTEST_SHARD_INDEX", "98")
    seen = []

    def native_run(argv, *, cwd, env, timeout):
        fixture = Path(env["SYMBIAN_E32_TEST_IMAGE"])
        assert fixture != probe
        assert fixture.read_bytes() == probe.read_bytes()
        assert fixture.parent == cwd
        assert not any(key.startswith("GTEST_") for key in env)
        assert "SYMBIAN_E32_TEST_HASH" not in env
        assert timeout == 15
        name = Path(argv[0]).name
        count = dict(verification.ORACLES)[name]
        output = Path(argv[-1].removeprefix("--gtest_output=json:"))
        output.write_text(json.dumps(_report(count)))
        seen.append(name)
        return "test-double output"

    monkeypatch.setattr(verification, "run", native_run)
    report = verification.verify_probe(probe, build, tmp_path / "out")
    assert len(seen) == 5
    assert report["tests_passed"] == 28
    assert report["historical_image_validation_passed"]
    assert report["cpu_probe_verified"]
    assert report["eka2l1_process_verified"]
    assert report["kernel_exit_verified"]
    assert not report["symbian_loader_verified"]
    assert not report["runtime_verified"]
    assert Path(report["tested_copy"]).read_bytes() == probe.read_bytes()


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_EKA2L1_ORACLES_BUILD"),
    reason="Set SYMBIAN_EKA2L1_ORACLES_BUILD to run native oracles",
)
def test_real_native_verification_cli(probe, tmp_path, capsys):
    assert (
        main(
            [
                "toolchain",
                "verify-probe",
                str(probe),
                "--oracles-build",
                os.environ["SYMBIAN_EKA2L1_ORACLES_BUILD"],
                "--output",
                str(tmp_path / "results"),
            ]
        )
        == 0
    )
    result = json.loads(capsys.readouterr().out)["result"]
    assert result["tests_passed"] == 28
    assert result["cpu_backends"] == ["dyncom", "dynarmic"]
    assert result["code_load_addresses"] == [0x8000, 0x20000]
    assert result["eka2l1_process_verified"]
    assert result["kernel_exit_verified"]
    assert result["emulator_os_profile"] == "epoc10"
    assert not result["runtime_verified"]
    for oracle in result["oracles"]:
        assert Path(oracle["results"]).is_file()
        assert Path(oracle["log"]).is_file()
