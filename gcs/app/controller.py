"""Composition root — owns every subsystem and wires them together.

The desktop counterpart of the firmware's ``GroundStationApp``. The UI talks
*only* to this object: it reads telemetry via :meth:`snapshot`, drains the
message log via :meth:`drain_notices`, issues commands via :attr:`commands`,
and opens/closes links via :meth:`connect` / :meth:`disconnect`. Nothing in here
imports Qt, so the same controller is fully unit-testable headless.
"""
from __future__ import annotations

import queue
from typing import List

from ..domain.telemetry import Severity, StatusText, TelemetrySnapshot, _now_ms
from ..interfaces.ports import ITelemetryLink
from ..mavlink.command_service import CommandService
from ..mavlink.mission_service import MissionService
from ..mavlink.pymavlink_link import PymavlinkLink
from .link_manager import LinkManager
from .store import TelemetryStore


class GcsController:
    def __init__(self) -> None:
        self.store = TelemetryStore()
        self._notices: "queue.Queue[StatusText]" = queue.Queue()
        self._link_manager = LinkManager(self.store, self._post_notice)
        self.commands = CommandService(
            link_provider=lambda: self._link_manager.link,
            state_provider=self.store.snapshot,
            notify=self._post_notice,
        )
        self.mission = MissionService(
            link_provider=lambda: self._link_manager.link,
            notify=self._post_notice,
        )
        # feed the mission protocol every incoming message (MISSION_REQUEST/ACK)
        self._link_manager.set_observer(self.mission.on_message)

    # ── connection lifecycle ──────────────────────────────────────────────────
    def connect(self, connection_string: str, baud: int, label: str = "") -> None:
        self.disconnect()
        self.store.reset()
        link: ITelemetryLink = PymavlinkLink(connection_string, baud, label)
        self._post_notice(StatusText(Severity.NOTICE,
                                     f"Connecting to {label or connection_string}…",
                                     _now_ms(), True))
        self._link_manager.start(link)

    def disconnect(self) -> None:
        if self._link_manager.link is not None:
            self._post_notice(StatusText(Severity.NOTICE, "Disconnected", _now_ms(), True))
        self._link_manager.stop()

    @property
    def is_connected(self) -> bool:
        return self._link_manager.link is not None

    # ── reads for the UI ──────────────────────────────────────────────────────
    def snapshot(self) -> TelemetrySnapshot:
        return self.store.snapshot()

    def drain_notices(self) -> List[StatusText]:
        out: List[StatusText] = []
        while True:
            try:
                out.append(self._notices.get_nowait())
            except queue.Empty:
                break
        return out

    # ── internals ─────────────────────────────────────────────────────────────
    def _post_notice(self, st: StatusText) -> None:
        self._notices.put(st)
