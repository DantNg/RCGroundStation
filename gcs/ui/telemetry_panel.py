"""Sidebar readouts: flight mode, arming, battery, GPS, speeds and link health.

A passive view — :meth:`update_from` is called on the UI timer with the latest
snapshot. Mirrors the firmware's ``DashboardView`` sidebar.
"""
from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtGui import QFont
from PySide6.QtWidgets import QHBoxLayout, QLabel

from ..domain import flight_modes
from ..domain.telemetry import TelemetrySnapshot
from . import theme
from .widgets import Panel, Stat


class TelemetryPanel(Panel):
    def __init__(self, parent=None):
        super().__init__("VEHICLE", parent)
        body = self.body()

        # Mode + armed banner
        banner = QHBoxLayout()
        self._mode = QLabel("—")
        self._mode.setFont(QFont("Segoe UI", 18, QFont.Bold))
        self._armed = QLabel("DISARMED")
        self._armed.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self._armed.setFont(QFont("Segoe UI", 13, QFont.Bold))
        banner.addWidget(self._mode)
        banner.addWidget(self._armed, 1)
        body.addLayout(banner)

        self._stats = {
            "battery": Stat("Battery"),
            "current": Stat("Current"),
            "remaining": Stat("Remaining"),
            "gps": Stat("GPS"),
            "sats": Stat("Satellites"),
            "airspeed": Stat("Airspeed"),
            "groundspeed": Stat("Ground spd"),
            "altitude": Stat("Altitude (rel)"),
            "heading": Stat("Heading"),
            "climb": Stat("Climb"),
            "link": Stat("Link"),
        }
        for s in self._stats.values():
            body.addWidget(s)
        body.addStretch(1)

    def update_from(self, s: TelemetrySnapshot) -> None:
        mode_name = flight_modes.mode_name(s.mode.autopilot, s.mode.custom_mode)
        self._mode.setText(mode_name if s.heartbeat_seen else "—")
        if s.mode.armed:
            self._armed.setText("ARMED")
            self._armed.setStyleSheet(f"color: {theme.BAD};")
        else:
            self._armed.setText("DISARMED")
            self._armed.setStyleSheet(f"color: {theme.GOOD};")

        b = s.battery
        v_color = theme.BAD if (0 < b.voltage < 10.5) else theme.TEXT
        self._stats["battery"].set(f"{b.voltage:.2f} V" if b.voltage else "—", v_color)
        self._stats["current"].set(f"{b.current:.1f} A" if b.updated_ms else "—")
        if b.remaining >= 0:
            rc = theme.BAD if b.remaining < 20 else (theme.WARN if b.remaining < 40 else theme.TEXT)
            self._stats["remaining"].set(f"{b.remaining}%", rc)
        else:
            self._stats["remaining"].set("—")

        g = s.gps
        gc = theme.GOOD if g.fix_type >= 3 else (theme.WARN if g.fix_type == 2 else theme.BAD)
        self._stats["gps"].set(f"{g.fix_label}  hdop {g.hdop:.1f}" if g.updated_ms else "—", gc)
        self._stats["sats"].set(str(g.satellites) if g.updated_ms else "—")

        self._stats["airspeed"].set(f"{s.vfr.airspeed:.1f} m/s" if s.vfr.updated_ms else "—")
        self._stats["groundspeed"].set(f"{s.vfr.groundspeed:.1f} m/s" if s.vfr.updated_ms else "—")
        self._stats["altitude"].set(f"{s.position.alt_rel:.1f} m" if s.position.updated_ms else "—")
        self._stats["heading"].set(f"{int(round(s.position.heading_deg)) % 360:03d}°"
                                   if s.position.updated_ms else "—")
        cc = theme.TEXT if s.vfr.climb >= 0 else theme.WARN
        self._stats["climb"].set(f"{s.vfr.climb:+.1f} m/s" if s.vfr.updated_ms else "—", cc)

        link = s.link
        if link.link_up:
            self._stats["link"].set(f"UP · {link.frames_received} frames", theme.GOOD)
        else:
            self._stats["link"].set("DOWN" if s.heartbeat_seen else "waiting…", theme.BAD)
