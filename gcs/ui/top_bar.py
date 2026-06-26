"""Full-width top status bar + a small dock for the active view's controls.

The fly-view's top edge is a single frosted bar (:class:`StatusBar`) that mirrors
a modern ground station: the vehicle name on the left, and on the right the live
**battery / GPS / link** chips next to a **flight-mode pill** (click to pick a
mode) and an **arming pill** (click to arm/disarm). It announces intent through
Qt signals and the MainWindow routes those to the command service.

The per-view controls (the map's provider/3D/Plan/zoom row, or the camera's
device/start row) no longer live in the top bar; they sit in a small floating
:class:`ControlDock` in a corner of the view, which simply hosts whichever
control widget belongs to the primary view.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QFrame, QHBoxLayout, QLabel, QMenu, QPushButton,
                               QSizePolicy, QWidget)

from ..domain import flight_modes
from ..domain.telemetry import TelemetrySnapshot
from . import theme
from .acrylic import AcrylicFrame


def _mode_names() -> list[str]:
    seen: set[str] = set()
    out: list[str] = []
    for qm in list(flight_modes.QUICK_MODES) + list(flight_modes.EXTRA_MODES):
        if qm.mode_name not in seen:
            seen.add(qm.mode_name)
            out.append(qm.mode_name)
    for cname in flight_modes.ARDUCOPTER.by_id.values():
        if cname not in seen:
            seen.add(cname)
            out.append(cname)
    return out


class StatusBar(AcrylicFrame):
    arm_requested = Signal(bool)        # force
    disarm_requested = Signal(bool)     # force
    mode_requested = Signal(str)        # canonical mode name
    disconnect_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent, radius=16)
        self.setObjectName("StatusBar")
        self.setSizePolicy(QSizePolicy.Preferred, QSizePolicy.Maximum)
        self._connected = False
        self._armed = False

        row = QHBoxLayout(self)
        row.setContentsMargins(14, 6, 12, 6)
        row.setSpacing(10)

        # ── left: vehicle identity ───────────────────────────────────────────
        self._dot = QLabel("●")
        self._dot.setObjectName("ConnDot")
        self._dot.setStyleSheet(f"color: {theme.WARN}; font-size: 13px;")
        self._vehicle = QLabel("Vehicle")
        self._vehicle.setObjectName("Vehicle")
        self._tag = QLabel("FLIGHT VIEW")
        self._tag.setObjectName("ViewTag")
        row.addWidget(self._dot)
        row.addWidget(self._vehicle)
        row.addWidget(self._tag)
        row.addStretch(1)

        # ── right: status chips + mode/arm pills ─────────────────────────────
        self._mode_pill = QPushButton("—")
        self._mode_pill.setObjectName("ModePill")
        self._mode_pill.setCursor(Qt.PointingHandCursor)
        self._mode_pill.setToolTip("Flight mode — click to change")
        self._mode_menu = QMenu(self._mode_pill)
        for name in _mode_names():
            self._mode_menu.addAction(name, lambda n=name: self.mode_requested.emit(n))
        self._mode_pill.setMenu(self._mode_menu)

        self._arm_pill = QPushButton("DISARMED")
        self._arm_pill.setObjectName("ArmPill")
        self._arm_pill.setCursor(Qt.PointingHandCursor)
        self._arm_pill.setToolTip("Click to arm / disarm")
        self._arm_pill.clicked.connect(self._on_arm_clicked)

        self._gps = self._chip("GPS —")
        self._link = self._chip("NO LINK")
        self._batt = self._chip("BAT —")

        self._disc = QPushButton("⏏")
        self._disc.setObjectName("ChipDisconnect")
        self._disc.setCursor(Qt.PointingHandCursor)
        self._disc.setToolTip("Disconnect")
        self._disc.clicked.connect(self.disconnect_requested.emit)

        for wdg in (self._mode_pill, self._arm_pill, self._gps, self._link,
                    self._batt, self._disc):
            row.addWidget(wdg)

        self.set_connected(False)

    @staticmethod
    def _chip(text: str) -> QLabel:
        lbl = QLabel(text)
        lbl.setObjectName("StatChip")
        return lbl

    @staticmethod
    def _recolor(lbl: QLabel, text: str, color: str) -> None:
        lbl.setText(text)
        # keep the #StatChip background/border (from the app QSS); only tint text
        lbl.setStyleSheet(f"color: {color};")

    # ── external state ────────────────────────────────────────────────────────
    def set_compact(self, compact: bool) -> None:
        """On a narrow window drop the view tag to keep the bar readable."""
        self._tag.setVisible(not compact)

    def set_label(self, label: str) -> None:
        self._vehicle.setText(label or "Vehicle")

    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        self._mode_pill.setEnabled(connected)
        self._arm_pill.setEnabled(connected)
        self._disc.setVisible(connected)
        if not connected:
            self._dot.setStyleSheet(f"color: {theme.WARN}; font-size: 13px;")
            self._mode_pill.setText("—")
            self._set_armed(False)
            self._recolor(self._gps, "GPS —", theme.TEXT_DIM)
            self._recolor(self._link, "NO LINK", theme.BAD)
            self._recolor(self._batt, "BAT —", theme.TEXT_DIM)

    def update_from(self, s: TelemetrySnapshot) -> None:
        if not self._connected:
            return
        up = s.link.link_up
        self._dot.setStyleSheet(
            f"color: {theme.GOOD if up else theme.WARN}; font-size: 13px;")

        if s.heartbeat_seen:
            self._mode_pill.setText(
                flight_modes.mode_name(s.mode.autopilot, s.mode.custom_mode))
            self._set_armed(s.mode.armed)

        # battery
        pct = s.battery.remaining
        bcol = (theme.TEXT_DIM if pct < 0 else
                theme.BAD if pct < 20 else
                theme.WARN if pct < 40 else theme.GOOD)
        btxt = (f"{s.battery.voltage:.1f}V" +
                (f" · {pct}%" if pct >= 0 else ""))
        self._recolor(self._batt, btxt, bcol)

        # gps
        gcol = (theme.GOOD if s.gps.fix_type >= 3 else
                theme.WARN if s.gps.fix_type == 2 else theme.BAD)
        self._recolor(self._gps, f"GPS {s.gps.satellites} · {s.gps.fix_label}", gcol)

        # link
        self._recolor(self._link, "LINK" if up else "NO LINK",
                      theme.GOOD if up else theme.BAD)

    # ── arming ────────────────────────────────────────────────────────────────
    def _set_armed(self, armed: bool) -> None:
        if armed == self._armed and self._arm_pill.text() in ("ARMED", "DISARMED"):
            return
        self._armed = armed
        self._arm_pill.setText("ARMED" if armed else "DISARMED")
        self._arm_pill.setObjectName("DisarmPill" if armed else "ArmPill")
        self._arm_pill.style().unpolish(self._arm_pill)
        self._arm_pill.style().polish(self._arm_pill)

    def _on_arm_clicked(self) -> None:
        if self._armed:
            self.disarm_requested.emit(False)
        else:
            self.arm_requested.emit(False)


class ControlDock(AcrylicFrame):
    """A small frosted dock hosting the primary view's control row (map / camera)."""

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("TopBar")   # reuse the transparent-acrylic QSS
        self.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Maximum)
        self._map_ctrl: QWidget | None = None
        self._cam_ctrl: QWidget | None = None
        row = QHBoxLayout(self)
        row.setContentsMargins(10, 6, 10, 6)
        row.setSpacing(8)
        self._row = row

    def attach_controls(self, map_ctrl: QWidget, cam_ctrl: QWidget) -> None:
        self._map_ctrl = map_ctrl
        self._cam_ctrl = cam_ctrl
        self._row.addWidget(map_ctrl)
        self._row.addWidget(cam_ctrl)
        cam_ctrl.setVisible(False)

    def set_active(self, which: str) -> None:
        if self._map_ctrl is not None:
            self._map_ctrl.setVisible(which == "map")
        if self._cam_ctrl is not None:
            self._cam_ctrl.setVisible(which == "cam")
