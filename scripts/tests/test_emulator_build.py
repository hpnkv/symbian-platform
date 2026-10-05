"""Checks source reuse without allowing the driver to overwrite local edits."""

import importlib.util
import subprocess
import tempfile
import unittest
from pathlib import Path

_SPEC = importlib.util.spec_from_file_location(
    "emulator_build", Path(__file__).parents[1] / "build_emulator.py"
)
assert _SPEC is not None and _SPEC.loader is not None
_BUILD = importlib.util.module_from_spec(_SPEC)
_SPEC.loader.exec_module(_BUILD)


class EmulatorBuildTest(unittest.TestCase):
    """Exercise exact patch replay and native host selection."""

    def setUp(self):
        self.temporary = tempfile.TemporaryDirectory()
        self.addCleanup(self.temporary.cleanup)
        self.root = Path(self.temporary.name)
        self.source = self.root / "source"
        self.source.mkdir()
        self.git("init", "--quiet")
        self.git("config", "user.name", "Build test")
        self.git("config", "user.email", "test@example.invalid")
        (self.source / "input.txt").write_text("original\n")
        self.git("add", "input.txt")
        self.git("commit", "--quiet", "-m", "Input")
        self.patch = self.root / "change.patch"
        self.patch.write_text(
            "diff --git a/input.txt b/input.txt\n"
            "--- a/input.txt\n+++ b/input.txt\n"
            "@@ -1 +1 @@\n-original\n+patched\n"
        )

    def git(self, *arguments):
        return subprocess.run(
            ["git", "-C", str(self.source), *arguments],
            check=True,
            capture_output=True,
        ).stdout

    def test_patch_replay_does_not_modify_worktree_or_index(self):
        before = self.git("ls-files", "--stage")
        _BUILD.expected_diff(self.source, [self.patch])
        self.assertEqual(before, self.git("ls-files", "--stage"))
        self.assertEqual("original\n", (self.source / "input.txt").read_text())
        self.assertEqual(b"", self.git("status", "--porcelain"))

    def test_reuse_accepts_exact_patch_and_rejects_extra_edits(self):
        self.git("apply", str(self.patch))
        _BUILD.validate_source(self.source, [self.patch])
        (self.source / "input.txt").write_text("owner edit\n")
        with self.assertRaisesRegex(RuntimeError, "Unexpected tracked"):
            _BUILD.validate_source(self.source, [self.patch])
        self.assertEqual(
            "owner edit\n", (self.source / "input.txt").read_text()
        )

    def test_untracked_sources_rejected_and_retained(self):
        path = self.source / "owner.cc"
        path.write_text("owner code\n")
        with self.assertRaisesRegex(RuntimeError, "Unexpected untracked"):
            _BUILD.validate_source(self.source, [])
        self.assertEqual("owner code\n", path.read_text())

    def test_host_matrix_and_no_unsupported_fallback(self):
        for system in ("Linux", "Darwin"):
            for arch in ("arm64", "aarch64", "x86_64"):
                target, cpu = _BUILD.host(system, arch)
                self.assertEqual(
                    "macos" if system == "Darwin" else "linux", target
                )
                self.assertEqual(
                    "x86_64" if arch == "x86_64" else "aarch64", cpu
                )
        with self.assertRaises(ValueError):
            _BUILD.host("Windows", "x86_64")
        with self.assertRaises(ValueError):
            _BUILD.host("Linux", "armv7l")

    def test_ffmpeg_is_static_and_has_no_implicit_external_codecs(self):
        for system, arch in (("linux", "aarch64"), ("macos", "x86_64")):
            args = _BUILD.ffmpeg_arguments(
                self.source,
                self.root / "prefix with spaces",
                system,
                arch,
                "clang",
                "clang++",
            )
            self.assertIn(f"--arch={arch}", args)
            self.assertIn("--disable-autodetect", args)
            self.assertIn("--enable-static", args)
            self.assertIn("--disable-vulkan", args)
            self.assertIn("--enable-decoder=amrwb", args)
            self.assertIn("--enable-decoder=h264", args)
            self.assertFalse(
                any(x.startswith("--enable-nonfree") for x in args)
            )


if __name__ == "__main__":
    unittest.main()
