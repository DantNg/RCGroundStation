"""High-level vehicle commands (arm/disarm, mode changes, takeoff, fly-to).

This is the *policy* layer over ``ICommandSink``: it knows that arming is a
``MAV_CMD_COMPONENT_ARM_DISARM``, that "LOITER" maps to a Copter custom_mode,
and that "Fly To Here" is a GUIDED position target — but it does not know how
bytes reach the vehicle. It depends on the sink through a provider callable so it
never holds a stale reference across reconnects, reads live vehicle state through
another provider, and reports every action (and every failure) through the same
notice sink the decoder uses — so user-initiated errors show up in the message
log just like vehicle STATUSTEXT, Mission-Planner style.
"""
from __future__ import annotations

from typing import Callable, Optional

from pymavlink import mavutil

from ..domain import flight_modes
from ..domain.telemetry import Severity, StatusText, TelemetrySnapshot, _now_ms
from ..interfaces.ports import ICommandSink

MAV_MODE_FLAG_CUSTOM_MODE_ENABLED = 1
ARM_MAGIC_FORCE = 21196  # param2 value that forces (dis)arm past safety checks

LinkProvider = Callable[[], Optional[ICommandSink]]
StateProvider = Callable[[], TelemetrySnapshot]
NoticeSink = Callable[[StatusText], None]


class CommandService:
    def __init__(self, link_provider: LinkProvider,
                 state_provider: StateProvider,
                 notify: NoticeSink):
        self._link = link_provider
        self._state = state_provider
        self._notify = notify

    # ── arming ──────────────────────────────────────────────────────────────
    def arm(self, force: bool = False) -> None:
        self._send_arm(True, force)

    def disarm(self, force: bool = False) -> None:
        self._send_arm(False, force)

    def _send_arm(self, arm: bool, force: bool) -> None:
        sink = self._require_link()
        if sink is None:
            return
        verb = "ARM" if arm else "DISARM"
        try:
            sink.command_long(
                mavutil.mavlink.MAV_CMD_COMPONENT_ARM_DISARM,
                1.0 if arm else 0.0,
                float(ARM_MAGIC_FORCE) if force else 0.0,
            )
            self._info(f"{verb} command sent" + (" (forced)" if force else ""))
        except Exception as exc:  # pragma: no cover - hardware/runtime errors
            self._error(f"{verb} failed: {exc}")

    # ── flight modes ─────────────────────────────────────────────────────────
    def set_mode_by_name(self, mode_name: str) -> None:
        sink = self._require_link()
        if sink is None:
            return
        self._switch_mode(sink, mode_name, announce=True)

    # ── takeoff (GUIDED) ──────────────────────────────────────────────────────
    def takeoff(self, altitude_m: float) -> None:
        sink = self._require_link()
        if sink is None:
            return
        if not self._state().mode.armed:
            self._error("Takeoff refused: vehicle is DISARMED — arm first")
            return
        self._ensure_guided(sink)
        try:
            sink.command_long(mavutil.mavlink.MAV_CMD_NAV_TAKEOFF,
                              0.0, 0.0, 0.0, 0.0, 0.0, 0.0, float(altitude_m))
            self._info(f"Takeoff to {altitude_m:.0f} m")
        except Exception as exc:  # pragma: no cover
            self._error(f"Takeoff failed: {exc}")

    # ── fly to here (GUIDED position target) ──────────────────────────────────
    def fly_to(self, lat: float, lon: float, alt_rel: float) -> None:
        sink = self._require_link()
        if sink is None:
            return
        if not self._state().mode.armed:
            self._notify(StatusText(Severity.WARNING,
                                    "Fly-to sent while DISARMED — vehicle will ignore it",
                                    _now_ms(), True))
        self._ensure_guided(sink)
        try:
            sink.set_position_target_global(lat, lon, alt_rel)
            self._info(f"Fly to {lat:.6f}, {lon:.6f} @ {alt_rel:.0f} m")
        except Exception as exc:  # pragma: no cover
            self._error(f"Fly-to failed: {exc}")

    # ── run an uploaded mission (AUTO) ────────────────────────────────────────
    def start_mission(self) -> None:
        sink = self._require_link()
        if sink is None:
            return
        if not self._state().mode.armed:
            self._notify(StatusText(Severity.WARNING,
                                    "Start mission sent while DISARMED — arm first",
                                    _now_ms(), True))
        self._switch_mode(sink, "AUTO", announce=True)
        try:
            sink.command_long(mavutil.mavlink.MAV_CMD_MISSION_START, 0.0, 0.0)
            self._info("Mission start (AUTO)")
        except Exception as exc:  # pragma: no cover
            self._error(f"Mission start failed: {exc}")

    # ── helpers ───────────────────────────────────────────────────────────────
    def _ensure_guided(self, sink: ICommandSink) -> None:
        """Switch to GUIDED if we aren't already — guided commands need it."""
        s = self._state()
        current = flight_modes.mode_name(s.mode.autopilot, s.mode.custom_mode)
        if current != "GUIDED":
            self._switch_mode(sink, "GUIDED", announce=False)
            self._info("Mode → GUIDED (required for guided command)")

    def _switch_mode(self, sink: ICommandSink, mode_name: str, announce: bool) -> None:
        autopilot = self._state().mode.autopilot
        custom_mode = flight_modes.mode_id(autopilot, mode_name)
        if custom_mode is None:
            self._error(f"Unknown mode '{mode_name}' for this autopilot")
            return
        try:
            sink.set_mode(MAV_MODE_FLAG_CUSTOM_MODE_ENABLED, custom_mode)
            if announce:
                self._info(f"Mode → {mode_name}")
        except Exception as exc:  # pragma: no cover
            self._error(f"Set mode {mode_name} failed: {exc}")

    def _require_link(self) -> Optional[ICommandSink]:
        sink = self._link()
        if sink is None:
            self._error("Not connected — command ignored")
        return sink

    def _info(self, text: str) -> None:
        self._notify(StatusText(Severity.NOTICE, text, _now_ms(), True))

    def _error(self, text: str) -> None:
        self._notify(StatusText(Severity.ERROR, text, _now_ms(), True))
