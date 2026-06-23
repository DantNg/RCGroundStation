"""Command panel: arm / disarm and a compact flight-mode picker.

The view is decoupled from the command service via Qt signals — it announces
*intent* (``arm_requested`` / ``mode_requested``) and the MainWindow routes that
to :class:`~gcs.mavlink.command_service.CommandService`. To keep the panel small
on the fly-view, flight modes are chosen from a **combobox + SET MODE** button
rather than a wall of buttons. The combobox follows the vehicle's live mode
(read back from HEARTBEAT) so it always reflects the real state — but it never
overrides a selection while its dropdown is open, so picking a target is smooth.
"""
from __future__ import annotations

from PySide6.QtWidgets import (QCheckBox, QComboBox, QInputDialog, QLabel,
                               QMessageBox, QPushButton)
from PySide6.QtCore import Qt, Signal

from ..domain import flight_modes
from ..domain.telemetry import TelemetrySnapshot
from .widgets import Panel


def _mode_choices():
    """Ordered (name) list: curated quick modes first, then the rest of the table."""
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
        self._live_mode: str | None = None
        self._armed = False

        # ── single arm/disarm toggle (reflects the live armed state) ───────
        self._arm_toggle = QPushButton("ARM")
        self._arm_toggle.setObjectName("Arm")
        self._arm_toggle.setMinimumHeight(42)
        self._arm_toggle.clicked.connect(self._on_arm_toggle)
        body.addWidget(self._arm_toggle)

        self._force = QCheckBox("Force (skip pre-arm checks)")
        body.addWidget(self._force)

        # ── takeoff (GUIDED) ───────────────────────────────────────────────
        self._takeoff = QPushButton("TAKEOFF")
        self._takeoff.setObjectName("Mode")
        self._takeoff.setToolTip("Arm + GUIDED required. Prompts for target altitude.")
        self._takeoff.clicked.connect(self._on_takeoff)
        body.addWidget(self._takeoff)

        # ── flight-mode picker (combobox + apply) ──────────────────────────
        self._mode_title = QLabel("FLIGHT MODE")
        self._mode_title.setObjectName("PanelTitle")
        body.addWidget(self._mode_title)

        self._mode_combo = QComboBox()
        descriptions = {qm.mode_name: qm.description
                        for qm in flight_modes.QUICK_MODES + flight_modes.EXTRA_MODES}
        for name in _mode_choices():
            self._mode_combo.addItem(name, name)
            if name in descriptions:
                self._mode_combo.setItemData(
                    self._mode_combo.count() - 1, descriptions[name], Qt.ToolTipRole)
        body.addWidget(self._mode_combo)

        self._set_mode = QPushButton("SET MODE")
        self._set_mode.setObjectName("Mode")
        self._set_mode.setToolTip("Switch the vehicle to the selected flight mode")
        self._set_mode.clicked.connect(self._on_set_mode)
        body.addWidget(self._set_mode)

        body.addStretch(1)
        self.set_connected(False)

    # ── external state ─────────────────────────────────────────────────────
    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        self._arm_toggle.setEnabled(connected)
        self._takeoff.setEnabled(connected)
        self._mode_combo.setEnabled(connected)
        self._set_mode.setEnabled(connected)
        if not connected:
            self._set_armed_ui(False)

    def set_compact(self, compact: bool) -> None:
        """On small screens, drop the secondary controls to save height."""
        self._force.setVisible(not compact)
        self._mode_title.setVisible(not compact)

    def update_from(self, s: TelemetrySnapshot) -> None:
        armed = s.mode.armed if s.heartbeat_seen else False
        if armed != self._armed:
            self._set_armed_ui(armed)

        active = flight_modes.mode_name(s.mode.autopilot, s.mode.custom_mode) \
            if s.heartbeat_seen else None
        if active == self._live_mode:
            return
        self._live_mode = active
        # follow the vehicle's live mode, but don't yank the user's open dropdown
        if active is None or self._mode_combo.view().isVisible():
            return
        idx = self._mode_combo.findData(active)
        if idx < 0:
            self._mode_combo.addItem(active, active)
            idx = self._mode_combo.findData(active)
        self._mode_combo.blockSignals(True)
        self._mode_combo.setCurrentIndex(idx)
        self._mode_combo.blockSignals(False)

    def _set_armed_ui(self, armed: bool) -> None:
        """Recolour/relabel the toggle: green ARM when safe, red DISARM when hot."""
        self._armed = armed
        self._arm_toggle.setText("DISARM" if armed else "ARM")
        self._arm_toggle.setObjectName("Disarm" if armed else "Arm")
        # re-apply the object-name-specific QSS (Qt needs an explicit re-polish)
        self._arm_toggle.style().unpolish(self._arm_toggle)
        self._arm_toggle.style().polish(self._arm_toggle)

    # ── handlers ────────────────────────────────────────────────────────────
    def _on_arm_toggle(self) -> None:
        if self._armed:
            self.disarm_requested.emit(self._force.isChecked())
        elif self._confirm("Arm the vehicle?",
                           "The motors may spin. Make sure the area is clear."):
            self.arm_requested.emit(self._force.isChecked())

    def _on_set_mode(self) -> None:
        name = self._mode_combo.currentData()
        if name:
            self.mode_requested.emit(name)

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
