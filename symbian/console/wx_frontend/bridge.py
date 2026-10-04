"""Run in-memory API requests outside the native GUI event loop."""

import asyncio
from collections.abc import Callable
from concurrent.futures import ThreadPoolExecutor
from typing import Any

import wx


class WxRequestBridge:
    """Serialize SDK requests and report completion on the wx main thread."""

    def __init__(self) -> None:
        self._executor = ThreadPoolExecutor(
            max_workers=1, thread_name_prefix="console"
        )
        self._closed = False

    def submit(
        self,
        operation: Callable[[], Any],
        completed: Callable[[Any, Exception | None], None],
    ) -> None:
        """Execute a coroutine-producing operation on the worker."""
        if self._closed:
            return

        def work() -> None:
            try:
                result = operation()
                if asyncio.iscoroutine(result):
                    result = asyncio.run(result)
                error = None
            except Exception as caught:
                result, error = None, caught
            if not self._closed:
                wx.CallAfter(completed, result, error)

        self._executor.submit(work)

    def close(self) -> None:
        """Stop accepting requests without waiting for device I/O on the UI."""
        self._closed = True
        self._executor.shutdown(wait=False, cancel_futures=True)
