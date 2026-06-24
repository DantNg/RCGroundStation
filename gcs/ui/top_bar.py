"""Combined compact top pill: connection status + the active view's controls.

Instead of a full-width map strip *and* a separate connection chip, the fly-view
folds both into one small rounded pill at the top-left. The connection segment
(status dot, link label, Disconnect) appears once a link is up; next to it sit
the controls for whichever view is primary — the map's provider/follow/zoom, or
the camera's device/start controls — so there is only ever one small bar to read.
The control widgets are *borrowed* from the MapWidget / CameraView (reparented in
here) so their wiring stays where it belongs.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QFrame, QHBoxLayout, QLabel, QPushButton,
                               QSizePolicy, QWidget)

from . import theme
from .acrylic import AcrylicFrame


class TopBar(AcrylicFrame):
    disconnect_requested = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setObjectName("TopBar")
        self.setSizePolicy(QSizePolicy.Maximum, QSizePolicy.Maximum)
        self._connected = False
        self._compact = False
        self._map_ctrl: QWidget | None = None
        self._cam_ctrl: QWidget | None = None

        row = QHBoxLayout(self)
        row.setContentsMargins(10, 4, 8, 4)
        row.setSpacing(8)
        self._row = row

        # ── connection segment (hidden until a link is up) ────────────────
        self._dot = QLabel("●")
        self._dot.setStyleSheet(f"color: {theme.WARN}; font-size: 13px;")
        self._link = QLabel("—")
        self._link.setObjectName("ConnLabel")
        self._disc = QPushButton("Disconnect")
        self._disc.setObjectName("ChipDisconnect")
        self._disc.setToolTip("Disconnect")
        self._disc.setCursor(Qt.PointingHandCursor)
        self._disc.clicked.connect(self.disconnect_requested.emit)
        self._sep = QFrame()
        self._sep.setFrameShape(QFrame.VLine)
        self._sep.setStyleSheet(f"color: {theme.PANEL_BORDER};")
        for wdg in (self._dot, self._link, self._disc, self._sep):
            row.addWidget(wdg)
        self._set_conn_visible(False)

    # ── borrow the per-view control widgets ─────────────────────────────────
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

    # ── connection state ─────────────────────────────────────────────────────
    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        self._set_conn_visible(connected)

    def set_state(self, link_up: bool, label: str) -> None:
        color = theme.GOOD if link_up else theme.WARN
        self._dot.setStyleSheet(f"color: {color}; font-size: 13px;")
        self._link.setText(label or ("Connected" if link_up else "Connecting…"))

    def set_compact(self, compact: bool) -> None:
        self._compact = compact
        # in tight layouts the dot carries link state and Disconnect shrinks to an
        # icon, so the combined pill stays narrow next to the control column.
        self._link.setVisible(self._connected and not compact)
        self._disc.setText("⏏" if compact else "Disconnect")
        if compact:
            self._row.setContentsMargins(8, 3, 6, 3)
            self._row.setSpacing(5)
        else:
            self._row.setContentsMargins(10, 4, 8, 4)
            self._row.setSpacing(8)

    # ── helpers ──────────────────────────────────────────────────────────────
    def _set_conn_visible(self, on: bool) -> None:
        self._dot.setVisible(on)
        self._link.setVisible(on and not self._compact)
        self._disc.setVisible(on)
        self._sep.setVisible(on)
