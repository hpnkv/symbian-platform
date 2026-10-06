"""Acquires pinned public inputs for the target SDK and resource compiler."""

import argparse
import hashlib
import json
import subprocess
from pathlib import Path

EXTRA = {
    "qt4": ("qt/qt", "1e0021d8d9e374ae3959fcd4eac5d9e7238cbc54"),
    "llvm-project": (
        "llvm/llvm-project",
        "85ac560262434c9ccfc0c183ec22d4138ed647fb",
    ),
    "abseil-cpp": (
        "abseil/abseil-cpp",
        "5650e9cf76d3be4318d5fa3af38ee483ddfd5e4a",
    ),
    "mimalloc": (
        "microsoft/mimalloc",
        "d4881d338125e1cb7c47ba4cfb398d6f7c0c8d45",
    ),
    "mm": (
        "SymbianSource/oss.FCL.sf.os.mm",
        "ebaa78373866f90dbf706e8d4eeb59ff65f1e107",
    ),
    "appsupport": (
        "SymbianSource/oss.FCL.sf.mw.appsupport",
        "3efd2b6c5ad920873846770a70f9769721e494c8",
    ),
    "zlib": ("madler/zlib", "51b7f2abdade71cd9bb0e7a373ef2610ec6f9daf"),
    "libpng": (
        "pnggroup/libpng",
        "4e3f57d50f552841550a36eabbb3fbcecacb7750",
    ),
}


def checkout(directory: Path, repository: str, revision: str) -> None:
    """Fetches one revision without resetting an existing research checkout."""
    if not directory.exists():
        directory.parent.mkdir(parents=True, exist_ok=True)
        subprocess.run(["git", "init", str(directory)], check=True)
        subprocess.run(
            [
                "git",
                "-C",
                str(directory),
                "remote",
                "add",
                "origin",
                f"https://github.com/{repository}.git",
            ],
            check=True,
        )
        subprocess.run(
            [
                "git",
                "-C",
                str(directory),
                "fetch",
                "--depth=1",
                "--filter=blob:none",
                "origin",
                revision,
            ],
            check=True,
        )
        if directory.name == "llvm-project":
            subprocess.run(
                [
                    "git",
                    "-C",
                    str(directory),
                    "sparse-checkout",
                    "set",
                    "libcxx",
                    "libcxxabi",
                    "compiler-rt",
                    "cmake",
                    "llvm/cmake",
                ],
                check=True,
            )
        subprocess.run(
            ["git", "-C", str(directory), "checkout", "--detach", "FETCH_HEAD"],
            check=True,
        )
    actual = subprocess.check_output(
        ["git", "-C", str(directory), "rev-parse", "HEAD"], text=True
    ).strip()
    if actual != revision:
        raise RuntimeError(
            f"Unexpected source revision at {directory}: {actual}"
        )


def main() -> None:
    """Prepares the exact source inputs consumed by native export."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--workspace", type=Path, default=Path.cwd())
    parser.add_argument("--resources-only", action="store_true")
    args = parser.parse_args()
    root = args.workspace.resolve()
    checkout(
        root / ".symbian/rcomp-epl-research",
        "SymbianRevive/symbian-build",
        "d3c2eadd3ff7826bdf9e1d92f447c357571af18b",
    )
    if args.resources_only:
        return
    profile = json.loads(
        (root / "research/gui_app/source-profile.json").read_text()
    )
    for name, revision in profile["repositories"].items():
        checkout(
            root / "research/upstream" / name,
            f"SymbianSource/oss.FCL.sf.os.{name}",
            revision,
        )
    for name, (repository, revision) in EXTRA.items():
        checkout(root / "research/upstream" / name, repository, revision)
    native_sources = json.loads(
        (root / "research/native-sdk/sources.json").read_text()
    )
    for name, source in native_sources["sources"].items():
        checkout(
            root / "research/upstream" / name,
            source["repository"],
            source["revision"],
        )
    nghttp2 = root / "third_party/nghttp2"
    if nghttp2.exists() and not (nghttp2 / ".git").exists():
        # A prepared release source snapshot is also valid. Verify the exact
        # inputs consumed by the build instead of consulting the parent repo.
        selection = json.loads(
            (root / "cpp/symbian/net/nghttp2-source.json").read_text()
        )
        for name, expected in selection["files"].items():
            actual = hashlib.sha256((nghttp2 / name).read_bytes()).hexdigest()
            if actual != expected:
                raise RuntimeError(f"nghttp2 source input mismatch: {name}")
    else:
        checkout(
            nghttp2,
            "nghttp2/nghttp2",
            "85e300c79fb6dbcfa9c1013215c8710c1c2cd3d2",
        )
    llvm = root / "research/upstream/llvm-project"
    for name in (
        "symbian-libcxx-lock-free.patch",
        "symbian-libcxx-chrono.patch",
        "symbian-compiler-rt-armv5-softdouble.patch",
    ):
        patch = root / "research/llvm" / name
        applied = subprocess.run(
            [
                "git",
                "-C",
                str(llvm),
                "apply",
                "--reverse",
                "--check",
                str(patch),
            ],
            capture_output=True,
        )
        if applied.returncode != 0:
            subprocess.run(
                ["git", "-C", str(llvm), "apply", "--check", str(patch)],
                check=True,
            )
            subprocess.run(
                ["git", "-C", str(llvm), "apply", str(patch)], check=True
            )


if __name__ == "__main__":
    main()
