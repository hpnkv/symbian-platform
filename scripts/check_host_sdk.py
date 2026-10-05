"""Builds and runs an ordinary consumer of a relocated standalone host SDK."""

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

_CMAKE = """cmake_minimum_required(VERSION 3.28)
project(host_sdk_consumer LANGUAGES CXX)
find_package(SymbianHost CONFIG REQUIRED)
add_executable(consumer main.cc)
target_link_libraries(consumer PRIVATE Symbian::Host)
"""
_SOURCE = r"""#include <string>
#include <openssl/rand.h>
#include "symbian/e32/e32.h"
#include "symbian/http/http1.h"
#include "symbian/sdk/exports.h"
#include "symbian/sis/sis.h"
#include "thread/fiber.h"
int main() {
  unsigned char random[16];
  if (RAND_bytes(random, sizeof(random)) != 1) return 5;
  auto exports = symbian::sdk::ParseExports("EXPORTS\nExample @ 7 NONAME\n");
  if (!exports.ok() || exports->size() != 1 || exports->front().ordinal != 7)
    return 1;
  auto response = symbian::http::internal::ParseResponseHead(
      "HTTP/1.1 200 OK\r\nContent-Length: 2\r\n\r\n");
  if (!response.ok() || response->status != 200) return 2;
  if (symbian::e32::InspectImage("broken").ok()) return 3;
  int value = 0;
  thread::Fiber fiber([&] { value = 42; });
  return fiber.Join().ok() && value == 42 ? 0 : 4;
}
"""


def check_native_tool(sdk: Path, root: Path, env: dict[str, str]) -> None:
    """Checks installed CLI validation and preservation of proxy inputs."""
    tool = sdk / "bin/symbian-native"
    subprocess.run([str(tool), "--help"], env=env, check=True, timeout=10)
    definition = root / "exports.def"
    original = b"EXPORTS\nExample @ 7 NONAME\n"
    definition.write_bytes(original)
    output = root / "generated proxy"
    proxy_args = [
        "proxy-sources",
        "--definition",
        str(definition),
        "--symbol",
        "Example",
        "--target-dll",
        "example.dll",
        "--output",
        str(output),
    ]
    subprocess.run([str(tool), *proxy_args], env=env, check=True, timeout=10)
    for name in ("exports.S", "exports.map", "proxy.ld"):
        if not (output / name).stat().st_size:
            raise RuntimeError(f"Missing generated proxy source: {name}")

    rejected = [
        [],
        ["unknown"],
        [*proxy_args, "--capabilities", "0"],
        [*proxy_args, "--output", str(output)],
        [*proxy_args, "--symbol"],
        [*proxy_args[:-1], "--target-dll"],
        [
            "convert-exe",
            "--input",
            str(definition),
            "--uid3",
            "4294967296",
            "--output",
            str(root / "invalid.exe"),
        ],
    ]
    for args in rejected:
        result = subprocess.run(
            [str(tool), *args],
            env=env,
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0 or "INVALID_ARGUMENT" not in result.stderr:
            raise RuntimeError(f"CLI accepted invalid options: {args}")
    if (root / "invalid.exe").exists():
        raise RuntimeError("Invalid conversion created an output")

    for kind in ("same-path", "hardlink", "symlink"):
        directory = root / kind
        directory.mkdir()
        protected = directory / "exports.S"
        if kind == "same-path":
            protected.write_bytes(original)
            input_path = protected
        else:
            input_path = definition
            if kind == "hardlink":
                os.link(definition, protected)
            else:
                protected.symlink_to(definition)
        args = proxy_args.copy()
        args[args.index("--definition") + 1] = str(input_path)
        args[args.index("--output") + 1] = str(directory)
        result = subprocess.run(
            [str(tool), *args],
            env=env,
            capture_output=True,
            text=True,
            timeout=10,
        )
        if result.returncode == 0 or "replace an input" not in result.stderr:
            raise RuntimeError(f"CLI failed to reject input alias: {kind}")
        if protected.read_bytes() != original:
            raise RuntimeError(f"CLI replaced an input: {kind}")
    if definition.read_bytes() != original:
        raise RuntimeError("CLI modified the export definition")


def main() -> None:
    """Checks headers, private dependency linkage, relocation and execution."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk", type=Path)
    args = parser.parse_args()
    sdk = args.sdk.resolve()
    env = {
        key: value
        for key, value in os.environ.items()
        if key not in ("SYMBIAN_DEPS_PREFIX", "CMAKE_PREFIX_PATH")
    }
    with tempfile.TemporaryDirectory(
        prefix="Symbian host SDK with spaces "
    ) as d:
        root = Path(d)
        relocated = root / "relocated SDK"
        shutil.copytree(sdk, relocated, symlinks=True)
        source = root / "consumer"
        source.mkdir()
        (source / "CMakeLists.txt").write_text(_CMAKE)
        (source / "main.cc").write_text(_SOURCE)
        build = root / "build"
        subprocess.run(
            [
                "cmake",
                "-S",
                str(source),
                "-B",
                str(build),
                "-G",
                "Ninja",
                f"-DCMAKE_PREFIX_PATH={relocated}",
                "-DCMAKE_DISABLE_FIND_PACKAGE_Boost=ON",
                "-DCMAKE_DISABLE_FIND_PACKAGE_OpenSSL=ON",
                "-DCMAKE_DISABLE_FIND_PACKAGE_absl=ON",
            ],
            env=env,
            check=True,
        )
        subprocess.run(["cmake", "--build", str(build)], env=env, check=True)
        subprocess.run(
            [str(build / "consumer")], env=env, check=True, timeout=30
        )
        check_native_tool(relocated, root, env)
    print("Relocated standalone host SDK consumer passed")


if __name__ == "__main__":
    main()
