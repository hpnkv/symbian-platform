"""Optional installed host archive consumer without Boost discovery."""

import os
import platform
import subprocess
from pathlib import Path

import pytest


@pytest.mark.skipif(
    not os.environ.get("SYMBIAN_HOST_CONCURRENCY_SDK"),
    reason="Set SYMBIAN_HOST_CONCURRENCY_SDK to an installed SDK",
)
def test_host_concurrency_archive_without_boost(tmp_path: Path) -> None:
    """The installed thread archive links without Boost targets or headers."""
    sdk = Path(os.environ["SYMBIAN_HOST_CONCURRENCY_SDK"]).resolve()
    workspace = Path(__file__).resolve().parents[2]
    archive = (
        sdk
        / "lib/host"
        / f"{platform.system()}-{platform.machine()}"
        / "libsymbian_host_primitives.a"
    )
    assert archive.is_file()
    assert (sdk / "licenses/Boost-BSL-1.0.txt").is_file()
    source = tmp_path / "source"
    source.mkdir()
    (source / "CMakeLists.txt").write_text(
        "cmake_minimum_required(VERSION 3.28)\n"
        "project(sdk_host_smoke LANGUAGES CXX)\n"
        "set(CMAKE_CXX_STANDARD 20)\n"
        "set(ABSL_PROPAGATE_CXX_STD ON)\n"
        "set(ABSL_BUILD_TESTING OFF)\n"
        f'add_subdirectory("{workspace / "research/upstream/abseil-cpp"}" '
        "abseil)\n"
        f'include("{sdk / "cmake/SymbianHostConcurrency.cmake"}")\n'
        "add_executable(sdk_host_smoke main.cc)\n"
        "target_link_libraries(sdk_host_smoke PRIVATE "
        "Symbian::HostConcurrency)\n"
    )
    (source / "main.cc").write_text(
        "#include <atomic>\n"
        "#include <chrono>\n"
        "#include <thread>\n"
        '#include "thread/fiber.h"\n'
        '#include "thread/boost_primitives.h"\n'
        '#include "thread/executor.h"\n'
        '#include "thread/select.h"\n'
        '#include "thread/selectables.h"\n'
        "int main() {\n"
        "  thread::Mutex mutex;\n"
        "  std::atomic<int> posts{0};\n"
        "  int value = 0;\n"
        "  thread::Fiber fiber([&] {\n"
        "    thread::MutexLock lock(&mutex);\n"
        "    value = 42;\n"
        "  });\n"
        "  thread::PermanentEvent event;\n"
        "  thread::Fiber notifier([&] {\n"
        "    thread::SleepFor(absl::Milliseconds(2));\n"
        "    event.Notify();\n"
        "  });\n"
        "  const int selected = thread::SelectUntil(\n"
        "      absl::Now() + absl::Seconds(1), {event.OnEvent()});\n"
        "  thread::Post([&] { posts.fetch_add(1); });\n"
        "  thread::PostAfter(absl::Milliseconds(5), "
        "[&] { posts.fetch_add(1); });\n"
        "  const auto until = std::chrono::steady_clock::now() + "
        "std::chrono::seconds(2);\n"
        "  while (posts.load() != 2 && "
        "std::chrono::steady_clock::now() < until) {\n"
        "    std::this_thread::sleep_for(std::chrono::milliseconds(1));\n"
        "  }\n"
        "  return fiber.Join().ok() && notifier.Join().ok() && selected == 0 "
        "&& value == 42 && posts.load() == 2 ? 0 : 1;\n"
        "}\n"
    )
    build = tmp_path / "build"
    subprocess.run(
        [
            "cmake",
            "-S",
            str(source),
            "-B",
            str(build),
            "-G",
            "Ninja",
            "-DCMAKE_DISABLE_FIND_PACKAGE_Boost=ON",
        ],
        check=True,
        capture_output=True,
        text=True,
        timeout=60,
    )
    subprocess.run(
        ["cmake", "--build", str(build), "--target", "sdk_host_smoke"],
        check=True,
        capture_output=True,
        text=True,
        timeout=180,
    )
    commands = subprocess.run(
        ["ninja", "-C", str(build), "-t", "commands", "sdk_host_smoke"],
        check=True,
        capture_output=True,
        text=True,
        timeout=30,
    ).stdout.splitlines()
    assert "libsymbian_host_primitives.a" in commands[-1]
    assert "libboost" not in commands[-1].lower()
    subprocess.run([str(build / "sdk_host_smoke")], check=True, timeout=30)
