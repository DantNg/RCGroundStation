"""Translate parsed MAVLink messages into telemetry-store updates.

Mirrors the firmware's ``TelemetryDecoder`` message-by-message. Telemetry goes
into the shared store; STATUSTEXT and COMMAND_ACK are surfaced as one-off
*notices* through an injected callback so the UI can show them in a
Mission-Planner-style message log (the decoder itself never touches the UI).
"""
from __future__ import annotations

from typing import Any, Callable, Optional

from pymavlink import mavutil

from ..domain.telemetry import Severity, StatusText, TelemetrySnapshot, _now_ms

MAV_MODE_FLAG_SAFETY_ARMED = 0x80

NoticeSink = Callable[[StatusText], None]

# MAV_RESULT → (human text, severity) for COMMAND_ACK feedback.
_RESULT_TEXT = {
    0: ("ACCEPTED", Severity.INFO),
    1: ("TEMPORARILY REJECTED", Severity.WARNING),
    2: ("DENIED", Severity.ERROR),
    3: ("UNSUPPORTED", Severity.ERROR),
    4: ("FAILED", Severity.ERROR),
    5: ("IN PROGRESS", Severity.NOTICE),
    6: ("CANCELLED", Severity.WARNING),
}


def _command_name(cmd: int) -> str:
    enums = mavutil.mavlink.enums.get("MAV_CMD", {})
    entry = enums.get(cmd)
    name = getattr(entry, "name", None) if entry else None
    return name.replace("MAV_CMD_", "") if name else f"CMD {cmd}"


def _as_text(value: Any) -> str:
    if isinstance(value, bytes):
        return value.split(b"\x00", 1)[0].decode("ascii", "replace")
    return str(value).split("\x00", 1)[0]


class TelemetryDecoder:
    """Stateless-ish decoder writing into a TelemetryStore."""

    def __init__(self, store, on_notice: Optional[NoticeSink] = None):
        self._store = store
        self._on_notice = on_notice or (lambda _st: None)

    def handle(self, msg: Any) -> None:
        mtype = msg.get_type()
        handler = self._HANDLERS.get(mtype)
        if handler is not None:
            handler(self, msg)

    # ── per-message handlers ────────────────────────────────────────────────
    def _heartbeat(self, m: Any) -> None:
        now = _now_ms()
        armed = (m.base_mode & MAV_MODE_FLAG_SAFETY_ARMED) != 0

        def mut(s: TelemetrySnapshot) -> None:
            s.mode.base_mode = m.base_mode
            s.mode.custom_mode = m.custom_mode
            s.mode.mav_type = m.type
            s.mode.autopilot = m.autopilot
            s.mode.system_status = m.system_status
            s.mode.armed = armed
            s.mode.updated_ms = now
            s.heartbeat_seen = True
            s.last_heartbeat_ms = now
        self._store.mutate(mut)

    def _attitude(self, m: Any) -> None:
        now = _now_ms()

        def mut(s: TelemetrySnapshot) -> None:
            s.attitude.roll = m.roll
            s.attitude.pitch = m.pitch
            s.attitude.yaw = m.yaw
            s.attitude.updated_ms = now
        self._store.mutate(mut)

    def _sys_status(self, m: Any) -> None:
        now = _now_ms()
        volt = m.voltage_battery / 1000.0 if m.voltage_battery != 0xFFFF else 0.0
        cur = 0.0 if m.current_battery < 0 else m.current_battery / 100.0

        def mut(s: TelemetrySnapshot) -> None:
            s.battery.voltage = volt
            s.battery.current = cur
            s.battery.remaining = m.battery_remaining
            s.battery.updated_ms = now
        self._store.mutate(mut)

    def _battery_status(self, m: Any) -> None:
        now = _now_ms()
        rem = m.battery_remaining

        def mut(s: TelemetrySnapshot) -> None:
            if rem >= 0:
                s.battery.remaining = rem
            s.battery.updated_ms = now
        self._store.mutate(mut)

    def _gps_raw(self, m: Any) -> None:
        now = _now_ms()
        hdop = 0.0 if m.eph == 0xFFFF else m.eph / 100.0

        def mut(s: TelemetrySnapshot) -> None:
            s.gps.fix_type = m.fix_type
            s.gps.satellites = m.satellites_visible
            s.gps.hdop = hdop
            s.gps.updated_ms = now
        self._store.mutate(mut)

    def _global_pos(self, m: Any) -> None:
        now = _now_ms()
        lat = m.lat / 1e7
        lon = m.lon / 1e7

        def mut(s: TelemetrySnapshot) -> None:
            s.position.lat = lat
            s.position.lon = lon
            s.position.alt_msl = m.alt / 1000.0
            s.position.alt_rel = m.relative_alt / 1000.0
            if m.hdg != 0xFFFF:
                s.position.heading_deg = m.hdg / 100.0
            s.position.valid = (lat != 0.0 or lon != 0.0)
            s.position.updated_ms = now
        self._store.mutate(mut)

    def _vfr_hud(self, m: Any) -> None:
        now = _now_ms()

        def mut(s: TelemetrySnapshot) -> None:
            s.vfr.airspeed = m.airspeed
            s.vfr.groundspeed = m.groundspeed
            s.vfr.climb = m.climb
            s.vfr.throttle = int(m.throttle)
            s.vfr.updated_ms = now
        self._store.mutate(mut)

    def _statustext(self, m: Any) -> None:
        try:
            sev = Severity(m.severity)
        except ValueError:
            sev = Severity.INFO
        st = StatusText(severity=sev, text=_as_text(m.text),
                        updated_ms=_now_ms(), valid=True)
        self._store.mutate(lambda s: setattr(s, "status", st))
        self._on_notice(st)

    def _command_ack(self, m: Any) -> None:
        text, sev = _RESULT_TEXT.get(m.result, (f"RESULT {m.result}", Severity.WARNING))
        notice = StatusText(
            severity=sev,
            text=f"{_command_name(m.command)}: {text}",
            updated_ms=_now_ms(),
            valid=True,
        )
        self._on_notice(notice)

    _HANDLERS = {
        "HEARTBEAT": _heartbeat,
        "ATTITUDE": _attitude,
        "SYS_STATUS": _sys_status,
        "BATTERY_STATUS": _battery_status,
        "GPS_RAW_INT": _gps_raw,
        "GLOBAL_POSITION_INT": _global_pos,
        "VFR_HUD": _vfr_hud,
        "STATUSTEXT": _statustext,
        "COMMAND_ACK": _command_ack,
    }
