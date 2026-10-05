"""Builds the pinned SDK-compatible GPL emulator in a disposable workspace.

Acquisition, native FFmpeg and frontend builds are separate steps for CI reuse.
This builds executable inputs; it does not deploy Qt or publish an archive.
"""

import argparse
import json
import os
import platform
import subprocess
import tarfile
import tempfile
import time
import urllib.request
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
REVISION = "2594edf4d6bf55d7bd3f0b46250fe2318d4dc2e8"
QT_VERSION = "6.8.3"
SDL_VERSION = "2.30.11"
PATCHES = (
    "instance-root",
    "runtime-probe",
    "guest-debug-step",
    "guest-debug-library-query",
    "guest-debug-thumb32-breakpoint",
    "symbian101-experimental",
    "guest-thread-register",
    "guest-control",
    "firmware-import-bounds",
    "fbs-unsupported-request",
    "background-window",
    "dll-wsd-dyncom-exit",
    "belle-library-entry-start",
    "belle-library-load-prepare",
    "belle-thread-exit-reason",
    "dyncom-strexd-value",
    "v10-thread-exit-reason",
    "ntick-fast-counter-hal",
    "fast-counter-rate",
    "belle-recv-from-no-length",
    "belle-nonblocking-tcp",
    "belle-secure-random",
    "belle-recv-one-or-more-no-length",
    "distribution-build",
    "distribution-query",
    "distribution-sdl",
    "distribution-resources",
    "distribution-tcp-close",
    "distribution-control-start",
)
LIBRARIES = ("avformat", "avcodec", "swscale", "avutil", "swresample")


def run(command, **kwargs):
    """Runs an argument vector, stopping on a failed build step."""
    print("+", " ".join(map(str, command)), flush=True)
    return subprocess.run(list(map(str, command)), check=True, **kwargs)


def fetch(command):
    """Retries bounded remote Git transfers, retaining pinned revisions."""
    for attempt in range(3):
        try:
            return run(command, timeout=180)
        except (subprocess.CalledProcessError, subprocess.TimeoutExpired):
            if attempt == 2:
                raise
            time.sleep(5 * (attempt + 1))


def host(system=None, machine=None):
    """Returns the native distribution target; cross builds are unsupported."""
    system = system or platform.system()
    machine = machine or platform.machine()
    systems = {"Darwin": "macos", "Linux": "linux"}
    architectures = {
        "arm64": "aarch64",
        "aarch64": "aarch64",
        "x86_64": "x86_64",
    }
    if system not in systems or machine not in architectures:
        raise ValueError(f"Unsupported emulator build host: {system}/{machine}")
    return systems[system], architectures[machine]


def expected_diff(source, patches):
    """Replays patches in a temporary Git index without editing source files."""
    with tempfile.TemporaryDirectory(prefix="symbian-emulator-index-") as temp:
        environment = dict(os.environ, GIT_INDEX_FILE=str(Path(temp) / "index"))
        run(["git", "-C", source, "read-tree", "HEAD"], env=environment)
        for patch in patches:
            run(
                [
                    "git",
                    "-C",
                    source,
                    "apply",
                    "--cached",
                    *patch_arguments(patch),
                ],
                env=environment,
            )
        return subprocess.check_output(
            [
                "git",
                "-C",
                str(source),
                "diff",
                "--cached",
                "--binary",
                "HEAD",
                "--ignore-submodules=all",
            ],
            env=environment,
        )


def validate_source(source, patches, generated=()):
    """Rejects extra edits or missing patches before reusing a source tree."""
    actual = subprocess.check_output(
        [
            "git",
            "-C",
            str(source),
            "diff",
            "--binary",
            "HEAD",
            "--ignore-submodules=all",
        ]
    )
    if actual != expected_diff(source, patches):
        raise RuntimeError(
            f"Unexpected tracked changes at {source}; use a new workspace"
        )
    untracked = (
        subprocess.check_output(
            [
                "git",
                "-C",
                str(source),
                "ls-files",
                "--others",
                "--exclude-standard",
                "-z",
            ]
        )
        .decode()
        .split("\0")
    )
    if set(filter(None, untracked)) - set(generated):
        raise RuntimeError(
            f"Unexpected untracked files at {source}; use a new workspace"
        )


def patch_arguments(patch):
    """Maps a vendored dependency patch to its actual owning repository."""
    if isinstance(patch, tuple):
        path, directory = patch
        return [f"--directory={directory}", str(path)]
    return [str(patch)]


def source_patches(system):
    """Groups maintained patches by their owning Git repository."""
    patches = ROOT / "research/eka2l1"
    result = {".": [patches / (name + ".patch") for name in PATCHES]}
    result["src/external/ffmpeg"] = [patches / "ffmpeg-linux-compat.patch"]
    if system == "linux":
        result["src/external/dynarmic"] = [
            (patches / "mcl-integer-sequence.patch", "externals/mcl")
        ]
    return result


def acquire(workspace, system):
    """Creates a pinned source tree or verifies a previous driver checkout."""
    source = workspace / "source"
    groups = source_patches(system)
    fresh = not source.exists()
    if fresh:
        workspace.mkdir(parents=True, exist_ok=True)
        run(["git", "init", source])
        run(
            [
                "git",
                "-C",
                source,
                "remote",
                "add",
                "origin",
                "https://github.com/EKA2L1/EKA2L1",
            ]
        )
        fetch(
            ["git", "-C", source, "fetch", "--depth", "1", "origin", REVISION]
        )
        run(["git", "-C", source, "checkout", "--detach", REVISION])
        fetch(
            [
                "git",
                "-C",
                source,
                "submodule",
                "update",
                "--init",
                "--recursive",
                "--depth",
                "1",
                "--jobs",
                "4",
            ]
        )
    revision = subprocess.check_output(
        ["git", "-C", str(source), "rev-parse", "HEAD"], text=True
    ).strip()
    if revision != REVISION:
        raise RuntimeError(
            f"Unexpected emulator revision at {source}: {revision}"
        )
    modules = subprocess.check_output(
        ["git", "-C", str(source), "submodule", "status", "--recursive"],
        text=True,
    ).splitlines()
    for module in modules:
        if not module.startswith(" "):
            raise RuntimeError(f"Missing or changed pinned submodule: {module}")
    repositories = [".", *(module.split()[1] for module in modules)]
    for relative in repositories:
        directory = source / relative
        patches = groups.get(relative, [])
        if fresh:
            validate_source(directory, [])
            for patch in patches:
                run(["git", "-C", directory, "apply", *patch_arguments(patch)])
        validate_source(
            directory,
            patches,
            ("CMakePresets.json",) if relative == "." else (),
        )
    return source


def ffmpeg_arguments(source, prefix, system, arch, cc, cxx):
    """Preserves the accepted codec selection for both native architectures."""
    native_arch = "arm64" if system == "macos" and arch == "aarch64" else arch
    args = [
        str(source / "configure"),
        f"--prefix={prefix}",
        f"--cc={cc}",
        f"--cxx={cxx}",
        f"--arch={native_arch}",
        "--disable-shared",
        "--enable-static",
        "--enable-pic",
        "--enable-zlib",
        "--disable-yasm",
        "--disable-everything",
        "--disable-vulkan",
        "--disable-autodetect",
        "--disable-avdevice",
        "--disable-filters",
        "--disable-programs",
        "--disable-network",
        "--disable-avfilter",
        "--disable-postproc",
        "--disable-encoders",
        "--disable-doc",
        "--extra-cflags=-D__STDC_CONSTANT_MACROS -O3",
    ]
    choices = {
        "decoder": (
            "h264 mpeg4 h263 h263p mpeg2video mjpeg mjpegb aac aac_latm "
            "wavpack amrnb amrwb mp3 pcm_s16le pcm_s8"
        ),
        "demuxer": (
            "h264 m4v mp3 mpegvideo mpegps mjpeg mov avi aac amr "
            "pcm_s16le pcm_s8 wav"
        ),
        "encoder": "pcm_s16le",
        "muxer": "amr avi mp3 wav pcm_s16le pcm_s8 ogg",
        "parser": "h264 mpeg4video mpegvideo aac aac_latm mpegaudio",
        "protocol": "file",
    }
    for kind, components in choices.items():
        args.extend(f"--enable-{kind}={item}" for item in components.split())
    if system == "macos":
        args.append("--target-os=darwin")
    return args


def build_ffmpeg(workspace, system, arch, cc, cxx, jobs):
    """Builds FFmpeg out of tree, retaining licenses in its native prefix."""
    source = workspace / "source/src/external/ffmpeg"
    prefix = workspace / "dependencies/ffmpeg"
    build = workspace / "ffmpeg-build"
    build.mkdir(exist_ok=True)
    run(ffmpeg_arguments(source, prefix, system, arch, cc, cxx), cwd=build)
    run(["make", f"-j{jobs}"], cwd=build)
    run(["make", "install"], cwd=build)
    for library in LIBRARIES:
        if not (prefix / "lib" / f"lib{library}.a").is_file():
            raise RuntimeError(f"FFmpeg did not install {library}")
    licenses = prefix / "licenses"
    licenses.mkdir(exist_ok=True)
    for notice in source.glob("COPYING*"):
        (licenses / notice.name).write_bytes(notice.read_bytes())


def configure(
    workspace, system, arch, cc, cxx, qt_prefix, dependency_sources=None
):
    """Writes a concrete Ninja preset with an external native FFmpeg prefix."""
    source = workspace / "source"
    cache = {
        "CMAKE_BUILD_TYPE": "RelWithDebInfo",
        "CMAKE_C_COMPILER": cc,
        "CMAKE_CXX_COMPILER": cxx,
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON",
        "CMAKE_POLICY_VERSION_MINIMUM": "3.5",
        "CMAKE_PROJECT_EKA2L1_INCLUDE": str(
            ROOT / "research/eka2l1/project-runtime.cmake"
        ),
        "SYMBIAN_EMULATOR_FFMPEG_PREFIX": str(
            workspace / "dependencies/ffmpeg"
        ),
        "QT_DEFAULT_MAJOR_VERSION": "6",
        "EKA2L1_ENABLE_QT_CAMERA": "OFF",
        "EKA2L1_SCRIPTING_LUA": "OFF",
        "EKA2L1_BUILD_TESTS": "ON",
    }
    if qt_prefix:
        cache["CMAKE_PREFIX_PATH"] = str(qt_prefix)
        candidates = [qt_prefix / "lib/cmake/Qt6"]
        if system == "linux":
            candidates.append(qt_prefix / f"lib/{arch}-linux-gnu/cmake/Qt6")
        qt_config = next(
            (
                path
                for path in candidates
                if (path / "Qt6Config.cmake").is_file()
            ),
            None,
        )
        if qt_config is not None:
            cache["QT_DIR"] = str(qt_config)
            cache["Qt6_DIR"] = str(qt_config)
    if system == "macos":
        cache["CMAKE_OSX_DEPLOYMENT_TARGET"] = "15.0"
        cache["CMAKE_OSX_ARCHITECTURES"] = (
            "arm64" if arch == "aarch64" else arch
        )
    sdl_prefix = workspace / "dependencies/sdl"
    if sdl_prefix.is_dir():
        cache["SYMBIAN_EMULATOR_SDL2_PREFIX"] = str(sdl_prefix)
    if dependency_sources:
        cache["FETCHCONTENT_FULLY_DISCONNECTED"] = "ON"
        for name in ("abseil", "nlohmann_json", "libuv"):
            path = dependency_sources / name
            if not path.is_dir():
                raise RuntimeError(f"Missing offline dependency source: {path}")
            cache[f"FETCHCONTENT_SOURCE_DIR_{name.upper()}"] = str(path)
    presets = {
        "version": 3,
        "configurePresets": [
            {
                "name": "sdk-emulator",
                "generator": "Ninja",
                "binaryDir": str(workspace / "build"),
                "cacheVariables": cache,
            }
        ],
        "buildPresets": [
            {
                "name": "sdk-emulator",
                "configurePreset": "sdk-emulator",
                "targets": ["eka2l1_qt", "symbian_firmware_tool", "ekatests"],
            }
        ],
        "testPresets": [
            {
                "name": "sdk-emulator",
                "configurePreset": "sdk-emulator",
                "output": {"outputOnFailure": True},
            }
        ],
    }
    path = source / "CMakePresets.json"
    content = json.dumps(presets, indent=2) + "\n"
    if path.exists() and path.read_text() != content:
        raise RuntimeError(
            "Preset changed; use a new workspace for different build inputs"
        )
    path.write_text(content)
    run(["cmake", "--preset", "sdk-emulator"], cwd=source)


def build_sdl(workspace, cc, cxx, jobs):
    """Compiles SDL from its pinned source instead of vendored host binaries."""
    archive = workspace / f"SDL2-{SDL_VERSION}.tar.gz"
    if not archive.is_file():
        urllib.request.urlretrieve(
            f"https://www.libsdl.org/release/SDL2-{SDL_VERSION}.tar.gz", archive
        )
    source = workspace / f"SDL2-{SDL_VERSION}"
    if not source.is_dir():
        with tarfile.open(archive) as bundle:
            bundle.extractall(workspace, filter="data")
    build = workspace / "sdl-build"
    run(
        [
            "cmake",
            "-S",
            source,
            "-B",
            build,
            "-G",
            "Ninja",
            f"-DCMAKE_INSTALL_PREFIX={workspace / 'dependencies/sdl'}",
            f"-DCMAKE_C_COMPILER={cc}",
            f"-DCMAKE_CXX_COMPILER={cxx}",
            "-DCMAKE_BUILD_TYPE=Release",
            "-DSDL_SHARED=OFF",
            "-DSDL_STATIC=ON",
            "-DSDL_TEST=OFF",
            "-DCMAKE_POSITION_INDEPENDENT_CODE=ON",
            "-DCMAKE_POLICY_VERSION_MINIMUM=3.5",
        ]
    )
    run(["cmake", "--build", build, "--parallel", jobs])
    run(["cmake", "--install", build])


def main():
    """Dispatches source, dependency, configure, build or test stages."""
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument(
        "stage",
        choices=(
            "acquire",
            "ffmpeg",
            "sdl",
            "configure",
            "build",
            "test",
            "all",
        ),
    )
    parser.add_argument("--workspace", required=True, type=Path)
    parser.add_argument("--cc", default="clang")
    parser.add_argument("--cxx", default="clang++")
    parser.add_argument("--qt-prefix", type=Path)
    parser.add_argument(
        "--source-tree",
        type=Path,
        help="Explicit patched source snapshot from a source archive",
    )
    parser.add_argument(
        "--dependency-sources",
        type=Path,
        help="Offline Abseil, JSON, libuv and SDL sources",
    )
    parser.add_argument("--jobs", type=int, default=min(os.cpu_count() or 2, 8))
    args = parser.parse_args()
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    workspace = args.workspace.resolve()
    system, arch = host()
    if system == "macos":
        os.environ["MACOSX_DEPLOYMENT_TARGET"] = "15.0"
    # Validate sources before every stage, including cache reuse.
    if args.source_tree:
        source = args.source_tree.resolve()
        if not (source / "CMakeLists.txt").is_file():
            parser.error(
                "--source-tree must contain the patched EKA2L1 sources"
            )
        workspace.mkdir(parents=True, exist_ok=True)
        link = workspace / "source"
        if not link.exists():
            link.symlink_to(source, target_is_directory=True)
        elif link.resolve() != source:
            parser.error("Workspace already selects another source tree")
    else:
        source = acquire(workspace, system)
    dependencies = (
        args.dependency_sources.resolve() if args.dependency_sources else None
    )
    if dependencies:
        import shutil

        archive = dependencies / f"SDL2-{SDL_VERSION}.tar.gz"
        if not archive.is_file():
            parser.error("Offline SDL source archive is missing")
        shutil.copy(archive, workspace / archive.name)
    stages = (
        ("ffmpeg", "sdl", "configure", "build", "test")
        if args.stage == "all"
        else (args.stage,)
    )
    for stage in stages:
        if stage == "ffmpeg":
            build_ffmpeg(workspace, system, arch, args.cc, args.cxx, args.jobs)
        elif stage == "sdl":
            build_sdl(workspace, args.cc, args.cxx, args.jobs)
        elif stage == "configure":
            configure(
                workspace,
                system,
                arch,
                args.cc,
                args.cxx,
                args.qt_prefix,
                dependencies,
            )
        elif stage == "build":
            run(
                [
                    "cmake",
                    "--build",
                    "--preset",
                    "sdk-emulator",
                    "--parallel",
                    args.jobs,
                ],
                cwd=source,
            )
        elif stage == "test":
            run(["ctest", "--preset", "sdk-emulator"], cwd=source)


if __name__ == "__main__":
    main()
