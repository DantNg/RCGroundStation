"""Thread-safe telemetry store — the cross-thread hand-off point.

The link worker mutates it under a lock; the UI reads atomic snapshots. This is
the desktop analogue of the firmware's ``TelemetryStore`` (a FreeRTOS mutex
guarding one ``TelemetrySnapshot``).
"""
from __future__ import annotations

import threading
from typing import Callable

from ..domain.telemetry import TelemetrySnapshot


class TelemetryStore:
    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._snap = TelemetrySnapshot()

    def mutate(self, fn: Callable[[TelemetrySnapshot], None]) -> None:
        """Apply ``fn`` to the live snapshot while holding the lock."""
        with self._lock:
            fn(self._snap)

    def snapshot(self) -> TelemetrySnapshot:
        """Return an independent copy safe to read on another thread."""
        with self._lock:
            return self._snap.copy()

    def reset(self) -> None:
        with self._lock:
            self._snap = TelemetrySnapshot()
