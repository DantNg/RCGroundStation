"""Mission upload — drive the MAVLink MISSION protocol to a real vehicle.

The 2D/3D map plans a list of waypoints; this turns them into a flyable
ArduCopter mission (a home placeholder, a take-off, then the waypoints) and
uploads it with the standard handshake:

    GCS → MISSION_COUNT(n)
    veh → MISSION_REQUEST_INT(seq)   (repeated)
    GCS → MISSION_ITEM_INT(seq, …)
    veh → MISSION_ACK(result)

``upload()`` is called on the UI thread; ``on_message()`` is fed every incoming
message by the link worker thread, so the small bit of shared state is guarded by
a lock. Every step (and any rejection) is reported through the same notice sink
the rest of the app uses, so progress shows in the Mission-Planner-style log.
"""
from __future__ import annotations

import threading
from typing import Callable, List, Optional

from pymavlink import mavutil

from ..domain.telemetry import Severity, StatusText, _now_ms
from ..interfaces.ports import ICommandSink

LinkProvider = Callable[[], Optional[ICommandSink]]
NoticeSink = Callable[[StatusText], None]

_FRAME = mavutil.mavlink.MAV_FRAME_GLOBAL_RELATIVE_ALT_INT
_NAV_WAYPOINT = mavutil.mavlink.MAV_CMD_NAV_WAYPOINT
_NAV_TAKEOFF = mavutil.mavlink.MAV_CMD_NAV_TAKEOFF
_MISSION_ACCEPTED = mavutil.mavlink.MAV_MISSION_ACCEPTED


class MissionService:
    def __init__(self, link_provider: LinkProvider, notify: NoticeSink):
        self._link = link_provider
        self._notify = notify
        self._lock = threading.Lock()
        self._items: List[dict] = []
        self._active = False

    # ── public API (UI thread) ────────────────────────────────────────────────
    def upload(self, waypoints: List[dict], takeoff_alt: Optional[float] = None) -> None:
        """Build + start uploading a mission from planned waypoints.

        ``waypoints`` is a list of ``{"lat","lon","alt"}`` dicts (the map's
        mission). Returns immediately; the vehicle then pulls the items item by
        item through :meth:`on_message`.
        """
        sink = self._link()
        if sink is None:
            self._error("Not connected — mission upload ignored")
            return
        if len(waypoints) < 1:
            self._error("Add at least one waypoint before uploading")
            return
        items = self._build_items(waypoints, takeoff_alt)
        with self._lock:
            self._items = items
            self._active = True
        try:
            sink.mission_count(len(items))
            self._info(f"Uploading mission: {len(items)} items "
                       f"({len(waypoints)} waypoints + take-off)…")
        except Exception as exc:  # pragma: no cover - hardware/runtime errors
            with self._lock:
                self._active = False
            self._error(f"Mission upload failed to start: {exc}")

    # ── incoming protocol messages (link-worker thread) ───────────────────────
    def on_message(self, msg) -> None:
        t = msg.get_type()
        if t in ("MISSION_REQUEST_INT", "MISSION_REQUEST"):
            self._send_item(int(msg.seq))
        elif t == "MISSION_ACK":
            self._on_ack(msg)

    # ── internals ─────────────────────────────────────────────────────────────
    def _send_item(self, seq: int) -> None:
        with self._lock:
            if not self._active or not (0 <= seq < len(self._items)):
                return
            item = self._items[seq]
        sink = self._link()
        if sink is None:
            return
        try:
            sink.mission_item_int(**item)
        except Exception as exc:  # pragma: no cover
            self._error(f"Mission item {seq} failed: {exc}")

    def _on_ack(self, msg) -> None:
        with self._lock:
            if not self._active:
                return
            self._active = False
        if int(msg.type) == _MISSION_ACCEPTED:
            self._notify(StatusText(Severity.NOTICE, "Mission upload complete ✓",
                                    _now_ms(), True))
        else:
            self._error(f"Vehicle rejected the mission (MAV_MISSION result {msg.type})")

    def _build_items(self, waypoints: List[dict], takeoff_alt: Optional[float]) -> List[dict]:
        first = waypoints[0]
        if takeoff_alt is None:
            takeoff_alt = first.get("alt", 30.0)
        items: List[dict] = []

        def add(command, current, lat, lon, alt):
            items.append(dict(
                seq=len(items), frame=_FRAME, command=command, current=current,
                autocontinue=1, p1=0.0, p2=0.0, p3=0.0, p4=0.0,
                lat_i=int(lat * 1e7), lon_i=int(lon * 1e7), alt=float(alt),
                mission_type=0))

        # seq 0 — home placeholder (the autopilot overwrites it with its own home)
        add(_NAV_WAYPOINT, 1, first["lat"], first["lon"], 0.0)
        # seq 1 — take off straight up to the first waypoint's altitude
        add(_NAV_TAKEOFF, 0, 0.0, 0.0, takeoff_alt)
        # the planned route
        for wp in waypoints:
            add(_NAV_WAYPOINT, 0, wp["lat"], wp["lon"], wp.get("alt", takeoff_alt))
        return items

    def _info(self, text: str) -> None:
        self._notify(StatusText(Severity.NOTICE, text, _now_ms(), True))

    def _error(self, text: str) -> None:
        self._notify(StatusText(Severity.ERROR, text, _now_ms(), True))
