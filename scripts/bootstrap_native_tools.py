"""Downloads the pinned Linux LLVM tools and ICU runtime for native archives."""

import argparse
import hashlib
import os
import subprocess
import tempfile
from pathlib import Path

LLVM = {
    "x86_64": (
        "X64",
        "6382de1c1a210ce5a5cc49d18bc8444d137742e7cbf9b19f4ae602bb1ab52534",
    ),
    "aarch64": (
        "ARM64",
        "143308c82f8e21707be7fdc135d5e0ddd9a46a377ca9f38befc716fd842cb59b",
    ),
}
ICU = {
    "x86_64": (
        "https://archive.ubuntu.com/ubuntu",
        "amd64",
        "58a154f6307289813da2276f900498ef536ae7c0522d2cf31a3c3c5cf62dfd9a",
    ),
    "aarch64": (
        "https://ports.ubuntu.com",
        "arm64",
        "ac68372cf4a976e6a206858fd9b28c68e49d37d650b9b8653270038a6e7bc174",
    ),
}


def download(url: str, destination: Path, digest: str) -> None:
    """Downloads a build input and verifies its pinned SHA-256."""
    subprocess.run(
        [
            "curl",
            "--fail",
            "--location",
            "--retry",
            "3",
            url,
            "--output",
            str(destination),
        ],
        check=True,
    )
    with destination.open("rb") as source:
        actual = hashlib.file_digest(source, "sha256").hexdigest()
    if actual != digest:
        raise RuntimeError(f"SHA-256 mismatch: {url}")


def main() -> None:
    """Extracts only the compiler/helper tools and required resource headers."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--arch", choices=LLVM, required=True)
    args = parser.parse_args()
    output = args.output.resolve()
    if not (output / "bin/clang").is_file():
        architecture, digest = LLVM[args.arch]
        base = f"LLVM-23.1.2-Linux-{architecture}"
        url = f"https://github.com/llvm/llvm-project/releases/download/llvmorg-23.1.2/{base}.tar.zst"
        output.parent.mkdir(parents=True, exist_ok=True)
        with tempfile.TemporaryDirectory(dir=output.parent) as d:
            temporary = Path(d)
            archive = temporary / "llvm.tar.zst"
            download(url, archive, digest)
            members = subprocess.check_output(
                [
                    "tar",
                    "--use-compress-program=zstd --long=30",
                    "-tf",
                    str(archive),
                ],
                text=True,
            ).splitlines()
            tools = {
                "clang",
                "clang++",
                "clang-23",
                "clang-scan-deps",
                "ld.lld",
                "lld",
                "llvm-ar",
                "llvm-ranlib",
            }
            selected = []
            for member in members:
                path = Path(member)
                relative = path.relative_to(base)
                if (
                    relative.parent == Path("bin")
                    and relative.name in tools
                    or str(relative).startswith("lib/clang/")
                    and "include" in relative.parts
                    or relative.parent == Path("lib")
                    and ".so" in relative.name
                ):
                    selected.append(member)
            listing = temporary / "members.txt"
            listing.write_text("\n".join(selected) + "\n")
            subprocess.run(
                [
                    "tar",
                    "--use-compress-program=zstd --long=30",
                    "-xf",
                    str(archive),
                    "-C",
                    str(temporary),
                    "--no-recursion",
                    "--verbatim-files-from",
                    "-T",
                    str(listing),
                ],
                check=True,
            )
            (temporary / base).rename(output)
    if not (output / "icu/usr").exists():
        server, architecture, digest = ICU[args.arch]
        archive = output / "libicu70.deb"
        download(
            f"{server}/pool/main/i/icu/libicu70_70.1-2_{architecture}.deb",
            archive,
            digest,
        )
        subprocess.run(
            ["dpkg-deb", "--extract", str(archive), str(output / "icu")],
            check=True,
        )
        archive.unlink()
    if "GITHUB_ENV" in os.environ:
        lib = next((output / "icu/usr/lib").glob("*-linux-gnu"))
        with open(os.environ["GITHUB_ENV"], "a") as environment:
            environment.write(f"SYMBIAN_LLVM_BIN={output / 'bin'}\n")
            environment.write(f"LD_LIBRARY_PATH={lib}\n")


if __name__ == "__main__":
    main()
