"""Finds tested emulator artifacts for an exact source commit."""

import json
import os
import subprocess
import time


def api(endpoint):
    """Reads Actions state using the job's scoped GitHub token."""
    return json.loads(subprocess.check_output(["gh", "api", endpoint]))


def main():
    """Waits for an existing producer before requesting another native build."""
    repository = os.environ["GITHUB_REPOSITORY"]
    sha = os.environ["GITHUB_SHA"]
    required = {"emulator-corresponding-source"} | {
        f"emulator-bundle-{system}-{arch}"
        for system, arch in (
            ("linux", "x86_64"),
            ("linux", "aarch64"),
            ("macos", "x86_64"),
            ("macos", "arm64"),
        )
    }
    deadline = time.monotonic() + 150 * 60
    endpoint = f"repos/{repository}/actions/runs?head_sha={sha}&per_page=100"
    run_id = ""
    while True:
        runs = api(endpoint)["workflow_runs"]
        for candidate in runs:
            if (
                candidate["path"]
                not in {
                    ".github/workflows/emulator.yml",
                    ".github/workflows/emulator-release.yml",
                }
                or candidate["conclusion"] != "success"
            ):
                continue
            available = {
                item["name"]
                for item in api(
                    f"repos/{repository}/actions/runs/{candidate['id']}/artifacts?per_page=100"
                )["artifacts"]
                if not item["expired"]
            }
            if required <= available:
                run_id = str(candidate["id"])
                break
        active = any(
            candidate["path"] == ".github/workflows/emulator.yml"
            and candidate["status"] != "completed"
            for candidate in runs
        )
        if run_id or not active or time.monotonic() >= deadline:
            break
        print("Waiting for the exact-source emulator producer", flush=True)
        time.sleep(30)
    with open(os.environ["GITHUB_OUTPUT"], "a") as output:
        output.write(f"run_id={run_id}\n")


if __name__ == "__main__":
    main()
