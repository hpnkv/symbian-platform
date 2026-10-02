"""Installed-wheel checks for A11-derived host scheduler/Python boundaries."""

import os
import subprocess
import sys
import textwrap
import zipfile
from pathlib import Path

import pytest

WHEEL = os.environ.get("SYMBIAN_CONCURRENCY_WHEEL")


@pytest.mark.skipif(not WHEEL, reason="Set SYMBIAN_CONCURRENCY_WHEEL")
def test_wheel_scheduler_park_releases_and_reacquires_gil(tmp_path):
    """A parked native fiber permits Python work and returns with GIL held."""
    with zipfile.ZipFile(WHEEL) as archive:
        archive.extractall(tmp_path)
    code = textwrap.dedent(
        """
        import sys
        import asyncio
        import threading
        import time

        sys.meta_path = [
            finder for finder in sys.meta_path
            if not type(finder).__module__.startswith("_editable_")
        ]
        sys.path.insert(0, sys.argv[1])
        from symbian import _native

        ready = threading.Event()
        observed = []

        def peer():
            ready.set()
            time.sleep(0.02)
            observed.append(time.monotonic())

        worker = threading.Thread(target=peer)
        worker.start()
        assert ready.wait(1)
        started = time.monotonic()
        assert _native._thread_park_probe(100) == (True, True, True)
        returned = time.monotonic()
        worker.join(timeout=1)
        assert 0.08 <= returned - started < 1
        assert observed and observed[0] < returned
        assert _native._deferred_ref_roundtrip(object()) == (1, 0)

        async def run_future():
            result = await asyncio.wait_for(
                _native._thread_post_after_future(20), timeout=1
            )
            assert result == 17
            cancelled = _native._thread_post_after_future(100)
            cancelled.cancel()
            try:
                await cancelled
            except asyncio.CancelledError:
                pass
            else:
                raise AssertionError("cancelled asyncio future completed")
            await asyncio.sleep(0.15)

        asyncio.run(run_future())
        assert _native._deferred_ref_roundtrip(object()) == (1, 0)
        """
    )
    result = subprocess.run(
        [sys.executable, "-c", code, str(tmp_path)],
        cwd=tmp_path,
        capture_output=True,
        text=True,
        timeout=20,
        check=False,
    )
    assert result.returncode == 0, result.stdout + result.stderr
