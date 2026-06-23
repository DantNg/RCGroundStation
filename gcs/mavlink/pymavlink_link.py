"""pymavlink-backed link implementing both port interfaces.

One adapter covers serial, UDP and TCP because pymavlink's
``mavutil.mavlink_connection`` abstracts the transport behind a connection
string. Receiving happens on the link-worker thread; sending happens on the UI
thread when a button is pressed, so every *write* is guarded by a lock (reads
and writes can otherwise interleave on the same connection and corrupt the
outgoing sequence).
"""
from __future__ import annotations

import threading
from typing import Any, Optional, Tuple

from pymavlink import mavutil

from ..interfaces.ports import ICommandSink, ITelemetryLink


class PymavlinkLink(ITelemetryLink, ICommandSink):
    """A single MAVLink connection (serial / UDP / TCP)."""

    # This GCS's own identity on the network.
    GCS_SYSTEM = 255
    GCS_COMPONENT = mavutil.mavlink.MAV_COMP_ID_MISSIONPLANNER \
        if hasattr(mavutil.mavlink, "MAV_COMP_ID_MISSIONPLANNER") else 190

    def __init__(self, connection_string: str, baud: int = 57600, label: str = ""):
        self._conn_str = connection_string
        self._baud = baud
        self._label = label or connection_string
        self._conn: Optional[Any] = None
        self._send_lock = threading.Lock()
        self._target_system = 0
        self._target_component = 0

    # ── ITelemetryLink ──────────────────────────────────────────────────────
    @property
    def is_open(self) -> bool:
        return self._conn is not None

    def open(self) -> None:
        # autoreconnect keeps serial links alive across USB re-enumeration.
        self._conn = mavutil.mavlink_connection(
            self._conn_str,
            baud=self._baud,
            source_system=self.GCS_SYSTEM,
            source_component=self.GCS_COMPONENT,
            autoreconnect=True,
        )

    def close(self) -> None:
        conn = self._conn
        self._conn = None
        if conn is not None:
            try:
                conn.close()
            except Exception:
                pass

    def recv(self, timeout: float) -> Optional[Any]:
        conn = self._conn
        if conn is None:
            return None
        msg = conn.recv_match(blocking=True, timeout=timeout)
        if msg is None:
            return None
        # Learn the vehicle identity from the first heartbeat it sends. Ignore
        # our own heartbeat echoes and other GCS components.
        if msg.get_type() == "HEARTBEAT" and msg.get_srcSystem() != self.GCS_SYSTEM:
            mav_type = getattr(msg, "type", 0)
            if mav_type != mavutil.mavlink.MAV_TYPE_GCS:
                self._target_system = msg.get_srcSystem()
                self._target_component = msg.get_srcComponent()
                conn.target_system = self._target_system
                conn.target_component = self._target_component
        return msg

    @property
    def source_name(self) -> str:
        return self._label

    @property
    def target(self) -> Tuple[int, int]:
        return (self._target_system, self._target_component)

    def request_data_streams(self, rate_hz: int = 12) -> None:
        conn = self._conn
        if conn is None or self._target_system == 0:
            return
        sys, comp = self._target_system, self._target_component
        att_hz = max(rate_hz, 10)
        # ArduPilot groups its messages into these legacy stream sets. EXTRA1 is
        # ATTITUDE — the artificial horizon — so it gets the highest rate.
        streams = (
            (mavutil.mavlink.MAV_DATA_STREAM_EXTRA1, att_hz),            # ATTITUDE
            (mavutil.mavlink.MAV_DATA_STREAM_EXTRA2, max(rate_hz // 2, 5)),  # VFR_HUD
            (mavutil.mavlink.MAV_DATA_STREAM_EXTRA3, 2),                 # AHRS/extras
            (mavutil.mavlink.MAV_DATA_STREAM_POSITION, 5),              # GLOBAL_POSITION_INT
            (mavutil.mavlink.MAV_DATA_STREAM_EXTENDED_STATUS, 2),       # SYS_STATUS, GPS_RAW
        )
        with self._send_lock:
            for stream_id, hz in streams:
                try:
                    conn.mav.request_data_stream_send(sys, comp, stream_id, hz, 1)
                except Exception:
                    pass
            # Belt-and-suspenders for stacks that prefer the modern command
            # (PX4 and newer ArduPilot honour MAV_CMD_SET_MESSAGE_INTERVAL).
            for msg_id, hz in (
                (mavutil.mavlink.MAVLINK_MSG_ID_ATTITUDE, att_hz),
                (mavutil.mavlink.MAVLINK_MSG_ID_VFR_HUD, max(rate_hz // 2, 5)),
                (mavutil.mavlink.MAVLINK_MSG_ID_GLOBAL_POSITION_INT, 5),
            ):
                try:
                    conn.mav.command_long_send(
                        sys, comp, mavutil.mavlink.MAV_CMD_SET_MESSAGE_INTERVAL, 0,
                        float(msg_id), float(int(1_000_000 / hz)), 0, 0, 0, 0, 0)
                except Exception:
                    pass

    # ── ICommandSink ────────────────────────────────────────────────────────
    def command_long(self, command: int, *params: float, confirmation: int = 0) -> None:
        sys, comp = self._require_target()
        p = list(params[:7]) + [0.0] * (7 - len(params))
        with self._send_lock:
            self._conn.mav.command_long_send(
                sys, comp, command, confirmation,
                p[0], p[1], p[2], p[3], p[4], p[5], p[6],
            )

    def set_mode(self, base_mode: int, custom_mode: int) -> None:
        sys, _ = self._require_target()
        with self._send_lock:
            self._conn.mav.set_mode_send(sys, base_mode, custom_mode)

    # type_mask with every bit set except the three position bits → "use
    # position only" (ignore velocity, acceleration, yaw and yaw-rate).
    _POS_ONLY_MASK = 0b0000111111111000

    def set_position_target_global(self, lat: float, lon: float, alt_rel: float) -> None:
        sys, comp = self._require_target()
        with self._send_lock:
            self._conn.mav.set_position_target_global_int_send(
                0,                      # time_boot_ms (0 = now)
                sys, comp,
                mavutil.mavlink.MAV_FRAME_GLOBAL_RELATIVE_ALT_INT,
                self._POS_ONLY_MASK,
                int(lat * 1e7), int(lon * 1e7), float(alt_rel),
                0.0, 0.0, 0.0,          # velocity
                0.0, 0.0, 0.0,          # acceleration
                0.0, 0.0,               # yaw, yaw_rate
            )

    def send_heartbeat(self) -> None:
        conn = self._conn
        if conn is None:
            return
        with self._send_lock:
            conn.mav.heartbeat_send(
                mavutil.mavlink.MAV_TYPE_GCS,
                mavutil.mavlink.MAV_AUTOPILOT_INVALID,
                0, 0, 0,
            )

    # ── helpers ─────────────────────────────────────────────────────────────
    def _require_target(self) -> Tuple[int, int]:
        if self._conn is None:
            raise RuntimeError("Link is not open")
        if self._target_system == 0:
            raise RuntimeError("No vehicle heartbeat yet — cannot target a command")
        return self._target_system, self._target_component
