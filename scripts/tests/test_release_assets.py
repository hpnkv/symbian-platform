"""Release gate regressions for incomplete and mixed-version artifacts."""

import importlib.util
import io
import tarfile
import tempfile
import unittest
from pathlib import Path

_SPEC = importlib.util.spec_from_file_location(
    "release_assets", Path(__file__).parents[1] / "check_release_assets.py"
)
assert _SPEC is not None and _SPEC.loader is not None
_ASSETS = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_ASSETS)


class ReleaseAssetsTest(unittest.TestCase):
    """Publication must reject partial, duplicated and stale matrices."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        root = Path(self.temporary.name)
        self.dist = root / "dist"
        self.assets = root / "assets"
        self.dist.mkdir()
        self.assets.mkdir()
        for python in _ASSETS.PYTHONS:
            for _, _, platform in _ASSETS.HOSTS:
                name = (
                    f"symbian_platform-0.1.0-{python}-{python}-{platform}.whl"
                )
                (self.dist / name).touch()
        self.archive(
            self.dist / "symbian_platform-0.1.0.tar.gz",
            "symbian_platform-0.1.0",
            [
                "VERSION",
                "cpp/python/CMakeLists.txt",
                "cpp/symbian/concurrency/upstream/cpp/thread/thread/fiber.h",
            ],
        )
        self.archive(
            self.assets / "symbian-source-0.1.0.tar.gz",
            "symbian-0.1.0",
            ["VERSION", "LICENSE", "CMakeLists.txt"],
        )
        for system, arch, _ in _ASSETS.HOSTS:
            self.archive(
                self.assets / f"symbian-host-0.1.0-{system}-{arch}.tar.gz",
                ".",
                [
                    "bin/symbian-native",
                    "lib/cmake/SymbianHost/SymbianHostConfig.cmake",
                    "share/symbian/LICENSE",
                    "share/symbian/VERSION",
                ],
            )

            self.archive(
                self.assets / f"symbian-sdk-0.1.0-{system}-{arch}.tar.gz",
                ".",
                [
                    "VERSION",
                    "sdk.json",
                    "bin/clang++",
                    "bin/ld.lld",
                    "bin/llvm-ar",
                    "bin/llvm-ranlib",
                    "bin/llvm-nm",
                    "bin/llvm-dwarfdump",
                    "bin/clang-scan-deps",
                    "bin/symbian-native",
                    "bin/rcomp",
                    "bin/uidcrc",
                    "bin/cmake",
                    "bin/ninja",
                    "cmake/SymbianApp.cmake",
                    "cmake/SymbianSdk.cmake",
                    "cmake/SymbianPlatform.cmake",
                    "cmake/exe_startup.S",
                    "cmake/exe_image.ld",
                    "cmake/eka1_startup.S",
                    "cmake/eka1_import_image.ld",
                    "proxies/euser-eka1/euser.dso",
                    "proxies/qtcore/qtcore.dso",
                    "proxies/qtgui/qtgui.dso",
                    "include/qt4/QtCore/qglobal.h",
                    "share/symbian/runtime/startup.S",
                    "share/symbian/runtime/startup.cc",
                    "share/symbian/runtime/image.ld",
                    "host/lib/cmake/SymbianHost/SymbianHostConfig.cmake",
                    "licenses/Symbian-Apache-2.0.txt",
                    "examples/hello_time/CMakeLists.txt",
                    "examples/qt_app/app.cc",
                    "examples/qt_app/CMakeLists.txt",
                    "examples/qt_app/symbian.toml",
                    "cmake/SymbianGraphics.cmake",
                    "cmake/modern_cpp.h",
                    "proxies/libglesv1_cm/libglesv1_cm.dso",
                    "proxies/libglesv2/libglesv2.dso",
                    "proxies/libegl/libegl.dso",
                    "include/graphics/GLES/gl.h",
                    "include/graphics/GLES2/gl2.h",
                    "include/graphics/EGL/egl.h",
                    "include/graphics/KHR/khrplatform.h",
                    "examples/gl_app/app.cc",
                    "examples/gl_app/CMakeLists.txt",
                    "examples/gl_app/symbian.toml",
                    "examples/gl_app/cube.cc",
                    "examples/gl_app/cube.h",
                    "examples/gl_app/exit_button.cc",
                    "examples/gl_app/exit_button.h",
                    "examples/gl_app/renderer.cc",
                    "examples/gl_app/renderer.h",
                    "examples/gl_app/shader.cc",
                    "examples/gl_app/shader.h",
                    "examples/gl_app/application.cc",
                    "examples/gl_app/application.h",
                    "examples/gl_app/shaders.h.in",
                    "examples/gl_app/shaders/cube.vert",
                    "examples/gl_app/shaders/cube.frag",
                    "examples/gl_app/shaders/exit_button.vert",
                    "examples/gl_app/shaders/exit_button.frag",
                    "examples/gl_app/sdk.cmake",
                    "examples/gl_app/sdk-run",
                    "examples/gl_app/sdk-debug",
                    "examples/gl_app/symbian-project.json",
                    "examples/gl_app/.idea/runConfigurations/Standalone_Run.xml",
                    "examples/gl_app/.idea/runConfigurations/Standalone_Debug.xml",
                    "examples/qt_app/sdk.cmake",
                    "examples/qt_app/sdk-run",
                    "examples/qt_app/sdk-debug",
                    "examples/qt_app/symbian-project.json",
                    "examples/qt_app/.idea/runConfigurations/Standalone_Run.xml",
                    "examples/qt_app/.idea/runConfigurations/Standalone_Debug.xml",
                    *(
                        f"lib/{target}/lib{component}.a"
                        for target in ("armv5t", "armv6")
                        for component in (
                            "symbian_guest_runtime",
                            "symbian_http",
                            "symbian_api_tls",
                            "symbian_api_websocket",
                        )
                    ),
                ],
            )

    @staticmethod
    def archive(path, prefix, members):
        with tarfile.open(path, "w:gz") as archive:
            for name in members:
                data = b"0.1.0\n" if name.endswith("VERSION") else b"fixture"
                member = tarfile.TarInfo(f"{prefix}/{name}")
                member.size = len(data)
                archive.addfile(member, io.BytesIO(data))

    def test_complete_matrix(self):
        _ASSETS.check(self.dist, self.assets, "0.1.0")

    def test_missing_python_target(self):
        next(self.dist.glob("*cp314*arm64.whl")).unlink()
        with self.assertRaisesRegex(ValueError, "Expected 16 wheels"):
            _ASSETS.check(self.dist, self.assets, "0.1.0")

    def test_duplicate_target_replaces_missing_architecture(self):
        wheel = next(self.dist.glob("*cp314*arm64.whl"))
        wheel.rename(
            self.dist
            / "symbian_platform-0.1.0-1-cp314-cp314-macosx_15_0_x86_64.whl"
        )
        with self.assertRaisesRegex(ValueError, "duplicate wheel"):
            _ASSETS.check(self.dist, self.assets, "0.1.0")

    def test_stale_release(self):
        wheel = next(self.dist.glob("*cp314*arm64.whl"))
        wheel.rename(wheel.with_name(wheel.name.replace("0.1.0", "0.0.9")))
        with self.assertRaisesRegex(ValueError, "Unexpected distribution"):
            _ASSETS.check(self.dist, self.assets, "0.1.0")

    def test_missing_native_tool(self):
        self.archive(
            self.assets / "symbian-host-0.1.0-linux-aarch64.tar.gz",
            ".",
            ["share/symbian/VERSION"],
        )
        with self.assertRaisesRegex(ValueError, "missing"):
            _ASSETS.check(self.dist, self.assets, "0.1.0")

    def test_missing_startup_sources(self):
        path = self.assets / "symbian-sdk-0.1.0-linux-x86_64.tar.gz"
        with tarfile.open(path) as archive:
            members = [
                member.name.removeprefix("./")
                for member in archive
                if not member.name.startswith("./share/symbian/runtime/")
            ]
        self.archive(path, ".", members)
        with self.assertRaisesRegex(ValueError, "share/symbian/runtime"):
            _ASSETS.check(self.dist, self.assets, "0.1.0")

    def test_incomplete_guest_payload(self):
        self.archive(
            self.assets / "symbian-sdk-0.1.0-macos-arm64.tar.gz",
            ".",
            ["VERSION", "sdk.json", "bin/clang++"],
        )
        with self.assertRaisesRegex(ValueError, "missing"):
            _ASSETS.check(self.dist, self.assets, "0.1.0")


if __name__ == "__main__":
    unittest.main()
