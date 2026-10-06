"""Builds GUI and shared-startup E32 images from a Python-free SDK."""

import argparse
import os
import shutil
import subprocess
import tempfile
from pathlib import Path

CMAKE = """cmake_minimum_required(VERSION 3.28)
project(hello_time LANGUAGES CXX ASM)
include(SymbianApp)
symbian_add_executable(hello_time app.cc model.cc app_bridge.cc)
target_link_libraries(hello_time PRIVATE
  Symbian::AbseilStatusOr Symbian::Stackless
  Symbian::WindowServer Symbian::Gdi)
target_compile_definitions(hello_time PRIVATE SYMBIAN_ENABLE_ABSEIL_STATUS=1
  SYMBIAN_ENABLE_TIMER_TASKS=1)
target_link_options(hello_time PRIVATE --gc-sections)
symbian_publish_executable(hello_time UID3 0xe0000830)
"""


STARTUP_CMAKE = """
symbian_add_executable(startup_check startup_check.cc)
target_link_libraries(startup_check PRIVATE Symbian::Runtime)
target_link_options(startup_check PRIVATE --gc-sections)
symbian_publish_executable(startup_check UID3 0xe0000831)
"""

STARTUP_SOURCE = """#include <string>
#include <vector>

namespace {
const std::string label = "native SDK startup";
}

int main() {
  const std::vector<int> values = {3, 5, 8};
  if (label != "native SDK startup" || values.size() != 3) {
    return 1;
  }
  return values[0] + values[1] == values[2] ? 0 : 2;
}
"""

QT_CMAKE = """
symbian_add_executable(qt_check qt_check.cc)
target_link_libraries(qt_check PRIVATE Symbian::Runtime Symbian::QtGui)
symbian_publish_executable(qt_check UID3 0xe0000832)
"""

EKA1_CMAKE = """cmake_minimum_required(VERSION 3.28)
project(eka1_check LANGUAGES CXX ASM)
include(SymbianPic)
symbian_add_executable(eka1_check main.cc)
target_link_libraries(eka1_check PRIVATE Symbian::Eka1EUser)
symbian_publish_executable(eka1_check UID3 0xe0000833)
"""

EKA1_SOURCE = """extern "C" void* Alloc(int size) asm("Alloc__4Useri");
extern "C" void Free(void* data) asm("Free__4UserPv");

int main() {
  void* allocation = Alloc(16);
  if (allocation == nullptr) {
    return -4;
  }
  Free(allocation);
  return 0;
}
"""


def write_example(directory: Path, root: Path) -> None:
    """Includes a complete GUI application's owned source in the archive."""
    directory.mkdir(parents=True)
    for name in (
        "app.cc",
        "model.cc",
        "model.h",
        "clock_time.h",
        "app_bridge.h",
        "app_bridge.cc",
    ):
        shutil.copy2(
            root / "symbian/project/templates" / name, directory / name
        )
    (directory / "CMakeLists.txt").write_text(CMAKE)
    (directory / "README.md").write_text(
        "# Build the native GUI example\n\n"
        "From the extracted SDK directory:\n\n```sh\n"
        "bin/cmake -S examples/hello_time -B build/hello_time -G Ninja "
        '-DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/symbian-arm.cmake" '
        '-DSYMBIAN_SDK_PREFIX="$PWD" '
        '-DCMAKE_MAKE_PROGRAM="$PWD/bin/ninja"\n'
        "bin/cmake --build build/hello_time\n```\n\n"
        "The output contains `e32/hello_time.exe` and its debug ELF. Add "
        "`-DSYMBIAN_TARGET_ARCH=armv5t` to select ARMv5T. "
        "This is an EKA2 application; compatible system DLLs must be supplied "
        "by the device firmware. Use the installed `symbian-platform` Python "
        "package to package, sign, stage, and run it.\n"
    )


def check(sdk: Path) -> None:
    """Moves the SDK before building both targets with only bundled tools."""
    with tempfile.TemporaryDirectory(prefix="native SDK relocation ") as d:
        root = Path(d)
        moved = root / "SDK with spaces"
        shutil.copytree(sdk, moved, symlinks=True)
        # Build another main() target through the same SDK startup machinery.
        example = moved / "examples/hello_time"
        (example / "startup_check.cc").write_text(STARTUP_SOURCE)
        shutil.copyfile(
            moved / "examples/qt_app/app.cc", example / "qt_check.cc"
        )
        with (example / "CMakeLists.txt").open("a") as cmake:
            cmake.write(STARTUP_CMAKE + QT_CMAKE)
        eka1 = root / "eka1 application"
        eka1.mkdir()
        (eka1 / "CMakeLists.txt").write_text(EKA1_CMAKE)
        (eka1 / "main.cc").write_text(EKA1_SOURCE)
        (eka1 / "symbian.toml").write_text(
            '[project]\nkind = "e32-eka1-import"\n'
        )
        env = {
            key: value
            for key, value in os.environ.items()
            if key
            not in (
                "SYMBIAN_LLVM_BIN",
                "LD_LIBRARY_PATH",
                "DYLD_LIBRARY_PATH",
                "CMAKE_PREFIX_PATH",
                "SYMBIAN_APP_SDK",
                "SYMBIAN_SDK_MANIFEST",
            )
        }
        env["PATH"] = f"{moved / 'bin'}:/usr/bin:/bin"
        env["HOME"] = str(root)
        env["XDG_CONFIG_HOME"] = str(root / "config")
        for architecture in ("armv5t", "armv6"):
            build = root / architecture
            subprocess.run(
                [
                    str(moved / "bin/cmake"),
                    "-S",
                    str(moved / "examples/hello_time"),
                    "-B",
                    str(build),
                    "-G",
                    "Ninja",
                    "-DCMAKE_TOOLCHAIN_FILE="
                    + str(moved / "cmake/symbian-arm.cmake"),
                    f"-DSYMBIAN_SDK_PREFIX={moved}",
                    f"-DSYMBIAN_TARGET_ARCH={architecture}",
                    f"-DCMAKE_MAKE_PROGRAM={moved / 'bin/ninja'}",
                ],
                env=env,
                check=True,
                timeout=120,
            )
            subprocess.run(
                [str(moved / "bin/cmake"), "--build", str(build)],
                env=env,
                check=True,
                timeout=180,
            )
            for name in ("hello_time", "startup_check", "qt_check"):
                image = (build / "e32" / f"{name}.exe").read_bytes()
                if image[16:20] != b"EPOC":
                    raise RuntimeError(f"Missing E32 signature in {name}")
        eka1_build = root / "eka1 build"
        subprocess.run(
            [
                str(moved / "bin/cmake"),
                "-S",
                str(eka1),
                "-B",
                str(eka1_build),
                "-G",
                "Ninja",
                f"-DCMAKE_TOOLCHAIN_FILE={moved}/cmake/symbian-arm.cmake",
                f"-DSYMBIAN_SDK_PREFIX={moved}",
                "-DSYMBIAN_TARGET_ARCH=armv5t",
                f"-DCMAKE_MAKE_PROGRAM={moved}/bin/ninja",
            ],
            env=env,
            check=True,
            timeout=120,
        )
        subprocess.run(
            [str(moved / "bin/cmake"), "--build", str(eka1_build)],
            env=env,
            check=True,
            timeout=180,
        )
        eka1_image = eka1_build / "e32/eka1_check.exe"
        if eka1_image.read_bytes()[16:20] != b"EPOC":
            raise RuntimeError("Missing E32 signature in EKA1 image")
        for tool in ("clang-scan-deps", "llvm-ar", "rcomp", "uidcrc"):
            # These helpers have different help exit conventions. A missing
            # dynamic loader or executable is a failure regardless of that.
            result = subprocess.run(
                [str(moved / "bin" / tool), "--help"],
                env=env,
                capture_output=True,
                timeout=15,
            )
            if result.returncode < 0 or result.returncode in (126, 127):
                raise RuntimeError(f"Bundled helper cannot run: {tool}")
    print(
        "Relocated native SDK: ARMv5T and ARMv6 GUI, Qt and shared-startup "
        "E32 builds, plus ARMv5T EKA1 imports, passed"
    )


def main() -> None:
    """Checks a assembled SDK without importing its Python bindings."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("sdk", type=Path)
    check(parser.parse_args().sdk.resolve())


if __name__ == "__main__":
    main()
