"""Left action/flight-mode rail — the vertical button strip on the fly-view.

Mirrors the NASA-style ground-station layout: a stack of big, glanceable
icon+label buttons down the left edge for the common in-flight actions (LAND,
RETURN, PAUSE) plus an ACTION button (take off), then a SINGLE/MULTI selector at
the bottom. The action buttons map to ArduCopter flight modes and light up when
that mode goes live (read back from HEARTBEAT); the view only announces intent
(``mode_requested`` / ``takeoff_requested``) and the MainWindow routes it to the
command service, so this stays a passive view.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QButtonGroup, QFrame, QPushButton, QSizePolicy,
                               QVBoxLayout)

from ..domain import flight_modes
from ..domain.telemetry import TelemetrySnapshot

# (glyph, label, mode-name) for the mode buttons — mode None means "take off".
_ACTIONS = [
    ("⬇", "LAND", "LAND"),
    ("↩", "RETURN", "RTL"),
    ("⏸", "PAUSE", "LOITER"),
    ("▶", "ACTION", None),
]


class ModeRail(QFrame):
    mode_requested = Signal(str)     # canonical mode name
    takeoff_requested = Signal()     # ACTION button — MainWindow prompts altitude

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("ModeRail")
        self.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Maximum)
        self._connected = False
        self._mode_buttons: dict[str, QPushButton] = {}

        col = QVBoxLayout(self)
        col.setContentsMargins(6, 6, 6, 6)
        col.setSpacing(8)

        for glyph, label, mode in _ACTIONS:
            btn = self._make_button(glyph, label)
            if mode is None:
                btn.clicked.connect(self.takeoff_requested.emit)
                btn.setToolTip("Take off (arm + GUIDED, prompts for altitude)")
                self._action_btn = btn
            else:
                btn.setCheckable(True)
                btn.clicked.connect(lambda _=False, m=mode: self.mode_requested.emit(m))
                btn.setToolTip(f"Switch to {mode}")
                self._mode_buttons[mode] = btn
            col.addWidget(btn)

        col.addSpacing(6)

        # single / multi vehicle selector (cosmetic — this build flies one vehicle)
        self._single = self._make_button("◈", "SINGLE")
        self._single.setCheckable(True)
        self._single.setChecked(True)
        self._multi = self._make_button("⧉", "MULTI")
        self._multi.setCheckable(True)
        self._multi.setEnabled(False)
        self._multi.setToolTip("Multi-vehicle control is not available in this build")
        grp = QButtonGroup(self)
        grp.setExclusive(True)
        grp.addButton(self._single)
        grp.addButton(self._multi)
        col.addWidget(self._single)
        col.addWidget(self._multi)

        self.set_connected(False)

    @staticmethod
    def _make_button(glyph: str, label: str) -> QPushButton:
        btn = QPushButton(f"{glyph}\n{label}")
        btn.setObjectName("RailBtn")
        btn.setCursor(Qt.PointingHandCursor)
        btn.setFixedSize(62, 56)
        return btn

    # ── external state ────────────────────────────────────────────────────────
    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        for btn in self._mode_buttons.values():
            btn.setEnabled(connected)
        self._action_btn.setEnabled(connected)
        if not connected:
            for btn in self._mode_buttons.values():
                btn.setChecked(False)

    def update_from(self, s: TelemetrySnapshot) -> None:
        active = (flight_modes.mode_name(s.mode.autopilot, s.mode.custom_mode)
                  if s.heartbeat_seen else None)
        for mode, btn in self._mode_buttons.items():
            btn.setChecked(mode == active)
