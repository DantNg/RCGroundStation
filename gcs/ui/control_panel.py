"""Command panel: arm / disarm and one-tap flight-mode buttons.

The view is decoupled from the command service via Qt signals — it announces
*intent* (``arm_requested`` / ``mode_requested``) and the MainWindow routes that
to :class:`~gcs.mavlink.command_service.CommandService`. The currently active
mode is highlighted by reading back HEARTBEAT telemetry, so the panel reflects
the vehicle's real state, not just what was clicked.
"""
from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (QCheckBox, QGridLayout, QHBoxLayout, QInputDialog,
                               QMessageBox, QPushButton)

from ..domain import flight_modes
from ..domain.telemetry import TelemetrySnapshot
from . import theme
from .widgets import Panel


class ControlPanel(Panel):
    arm_requested = Signal(bool)        # force
    disarm_requested = Signal(bool)     # force
    mode_requested = Signal(str)        # canonical mode name
    takeoff_requested = Signal(float)   # altitude (m)

    def __init__(self, parent=None):
        super().__init__("CONTROL", parent)
        body = self.body()
        self._connected = False
        self._last_takeoff_alt = 10.0

        # ── arm / disarm row ──────────────────────────────────────────────
        arm_row = QHBoxLayout()
        self._arm = QPushButton("ARM")
        self._arm.setObjectName("Arm")
        self._arm.clicked.connect(self._on_arm)
        self._disarm = QPushButton("DISARM")
        self._disarm.setObjectName("Disarm")
        self._disarm.clicked.connect(self._on_disarm)
        arm_row.addWidget(self._arm)
        arm_row.addWidget(self._disarm)
        body.addLayout(arm_row)

        self._force = QCheckBox("Force (skip pre-arm checks)")
        body.addWidget(self._force)

        # ── takeoff (GUIDED) ───────────────────────────────────────────────
        self._takeoff = QPushButton("TAKEOFF")
        self._takeoff.setObjectName("Mode")
        self._takeoff.setToolTip("Arm + GUIDED required. Prompts for target altitude.")
        self._takeoff.clicked.connect(self._on_takeoff)
        body.addWidget(self._takeoff)

        # ── flight-mode grid ──────────────────────────────────────────────
        grid = QGridLayout()
        grid.setSpacing(6)
        self._mode_buttons = {}
        self._extra_buttons = []
        extra_names = {qm.mode_name for qm in flight_modes.EXTRA_MODES}
        modes = list(flight_modes.QUICK_MODES) + list(flight_modes.EXTRA_MODES)
        for i, qm in enumerate(modes):
            btn = QPushButton(qm.label)
            btn.setObjectName("Mode")
            btn.setToolTip(qm.description)
            btn.setCheckable(True)
            btn.clicked.connect(lambda _checked, name=qm.mode_name: self.mode_requested.emit(name))
            self._mode_buttons[qm.mode_name] = btn
            if qm.mode_name in extra_names:
                self._extra_buttons.append(btn)
            grid.addWidget(btn, i // 2, i % 2)
        body.addLayout(grid)
        body.addStretch(1)

        self.set_connected(False)

    # ── external state ─────────────────────────────────────────────────────
    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        self._arm.setEnabled(connected)
        self._disarm.setEnabled(connected)
        self._takeoff.setEnabled(connected)
        for btn in self._mode_buttons.values():
            btn.setEnabled(connected)

    def set_compact(self, compact: bool) -> None:
        """On small screens, drop the secondary controls to save height."""
        self._force.setVisible(not compact)
        for btn in self._extra_buttons:
            btn.setVisible(not compact)

    def update_from(self, s: TelemetrySnapshot) -> None:
        active = flight_modes.mode_name(s.mode.autopilot, s.mode.custom_mode) \
            if s.heartbeat_seen else None
        for name, btn in self._mode_buttons.items():
            on = (name == active)
            btn.setChecked(on)
            # highlight the live mode with the accent colour
            btn.setStyleSheet(
                f"background-color: {theme.ACCENT}; color: #0d1117; border: none;"
                if on else "")

    # ── handlers ────────────────────────────────────────────────────────────
    def _on_arm(self) -> None:
        if self._confirm("Arm the vehicle?",
                         "The motors may spin. Make sure the area is clear."):
            self.arm_requested.emit(self._force.isChecked())

    def _on_disarm(self) -> None:
        self.disarm_requested.emit(self._force.isChecked())

    def _on_takeoff(self) -> None:
        alt, ok = QInputDialog.getDouble(
            self, "Takeoff", "Target altitude (m above home):",
            self._last_takeoff_alt, 1.0, 1000.0, 1)
        if not ok:
            return
        if not self._confirm("Take off?",
                             f"Vehicle will arm-check, switch to GUIDED and climb to "
                             f"{alt:.0f} m. Make sure the area is clear."):
            return
        self._last_takeoff_alt = alt
        self.takeoff_requested.emit(alt)

    def _confirm(self, title: str, text: str) -> bool:
        box = QMessageBox(self)
        box.setIcon(QMessageBox.Warning)
        box.setWindowTitle(title)
        box.setText(text)
        box.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        box.setDefaultButton(QMessageBox.No)
        return box.exec() == QMessageBox.Yes
