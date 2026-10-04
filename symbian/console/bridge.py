"""Keep the Tk event thread responsive during in-memory API calls."""

import asyncio
import queue
from collections.abc import Awaitable, Callable
from concurrent.futures import Future, ThreadPoolExecutor
from typing import Any


class TkRequestBridge:
    """Run one SDK request at a time and deliver results on the Tk thread."""

    def __init__(self, root: Any) -> None:
        self._root = root
        self._executor = ThreadPoolExecutor(
            max_workers=1, thread_name_prefix="symbian-console"
        )
        self._finished: queue.SimpleQueue[
            tuple[Future, Callable[[object | None, Exception | None], None]]
        ] = queue.SimpleQueue()
        self._active = 0
        self._closing = False
        self._after_id = root.after(40, self._drain)

    @property
    def busy(self) -> bool:
        """Whether a local SDK operation is still running."""
        return self._active > 0

    def submit(
        self,
        operation: Callable[[], Awaitable[object]],
        completed: Callable[[object | None, Exception | None], None],
    ) -> None:
        """Submit one coroutine to the in-memory HTTPX transport worker."""
        if self._closing:
            return
        self._active += 1
        future = self._executor.submit(lambda: asyncio.run(operation()))
        future.add_done_callback(
            lambda result: self._finished.put((result, completed))
        )

    def _drain(self) -> None:
        """Deliver queued completions without touching Tk from the worker."""
        while True:
            try:
                future, callback = self._finished.get_nowait()
            except queue.Empty:
                break
            self._active -= 1
            try:
                result = future.result()
            except Exception as error:
                callback(None, error)
            else:
                callback(result, None)
        if self._closing and not self.busy:
            self._executor.shutdown(wait=False, cancel_futures=True)
            self._root.after_cancel(self._after_id)
            self._root.destroy()
        else:
            self._after_id = self._root.after(40, self._drain)

    def close(self) -> None:
        """Finish an active operation before destroying the GUI."""
        self._closing = True
        if not self.busy:
            self._executor.shutdown(wait=False, cancel_futures=True)
            self._root.after_cancel(self._after_id)
            self._root.destroy()
