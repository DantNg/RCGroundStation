"""Main window — map-centric fly view with floating instrument overlays.

For autonomous work the **map is the workspace**: it fills the window, and a
small circular HUD, the control panel and the message log float on top, with
warnings surfaced over the map. The only object that knows about
:class:`GcsController`; a ~25 Hz timer polls the store, feeds the passive
overlays and drains notices into the log + on-map warning banners.
"""
from __future__ import annotations

from PySide6.QtCore import QTimer
from PySide6.QtWidgets import QVBoxLayout, QWidget

from ..app.controller import GcsController
from ..config import AppConfig
from . import theme
from .connection_bar import ConnectionBar
from .control_panel import ControlPanel
from .map_widget import MapWidget
from .messages_panel import MessagesPanel
from .overlay_stage import OverlayStage
from .round_hud import RoundHud
from .warning_overlay import WarningOverlay

_REFRESH_HZ = 25


def _clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


class MainWindow(QWidget):
    def __init__(self, controller: GcsController, config: AppConfig):
        super().__init__()
        self._controller = controller
        self._config = config

        self.setWindowTitle("Lite Ground Station — Desktop")
        self.resize(1320, 820)
        self.setMinimumSize(460, 320)
        self.setStyleSheet(theme.STYLESHEET)

        root = QVBoxLayout(self)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(6)

        # connection bar
        self._conn_bar = ConnectionBar(config)
        self._conn_bar.connect_requested.connect(self._on_connect)
        self._conn_bar.disconnect_requested.connect(self._on_disconnect)
        root.addWidget(self._conn_bar)

        # ── fly view: map base + floating overlays ────────────────────────────
        self._map = MapWidget()
        self._hud = RoundHud()
        self._control = ControlPanel()
        self._messages = MessagesPanel()
        self._warnings = WarningOverlay()

        self._control.arm_requested.connect(self._controller.commands.arm)
        self._control.disarm_requested.connect(self._controller.commands.disarm)
        self._control.mode_requested.connect(self._controller.commands.set_mode_by_name)
        self._control.takeoff_requested.connect(self._controller.commands.takeoff)
        self._map.fly_to_requested.connect(self._controller.commands.fly_to)

        self._stage = OverlayStage(self._map, self._layout_overlays)
        self._stage.add_overlay(self._hud)
        self._stage.add_overlay(self._control)
        self._stage.add_overlay(self._messages)
        self._stage.add_overlay(self._warnings)   # raised last → on top, click-through
        root.addWidget(self._stage, 1)

        # refresh timer
        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(int(1000 / _REFRESH_HZ))

    # ── overlay placement (called from the stage's resizeEvent) ───────────────
    def _layout_overlays(self, w: int, h: int) -> None:
        m = 8
        tiny = (w < 560 or h < 380)
        small = (w < 860 or h < 520)

        # round HUD — bottom-left
        hud_sz = int(_clamp(min(w * 0.30, h * 0.45), 150, 240))
        hud_h = min(hud_sz + 56, h - 2 * m)
        self._hud.setGeometry(m, h - hud_h - m, hud_sz, hud_h)

        # control panel — right column (below the map's own control strip)
        top_inset = 44
        cw = 150 if tiny else 210
        self._control.set_compact(small)
        self._control.setFixedWidth(cw)
        ch = min(self._control.sizeHint().height(), h - top_inset - m)
        self._control.setGeometry(w - cw - m, top_inset, cw, ch)

        # message log — bottom strip between HUD and control (hide if cramped)
        msg_left = m + hud_sz + m
        msg_right = w - cw - 2 * m
        msg_w = msg_right - msg_left
        msg_h = int(_clamp(h * 0.22, 90, 150))
        if msg_w >= 240 and not tiny:
            self._messages.setGeometry(msg_left, h - msg_h - m, msg_w, msg_h)
            self._messages.setVisible(True)
        else:
            self._messages.setVisible(False)

        # warning banners — top-centre of the map, clear of the control column
        avail = w - cw - 3 * m
        warn_w = int(_clamp(min(avail, 600), 220, 600))
        warn_x = m + max(0, (avail - warn_w) // 2)
        self._warnings.setGeometry(warn_x, top_inset, warn_w, int(_clamp(h * 0.4, 120, 200)))

    # ── connection ───────────────────────────────────────────────────────────
    def _on_connect(self, cfg: AppConfig) -> None:
        self._config = cfg
        cfg.save()
        self._controller.connect(cfg.connection_string(), cfg.baud, cfg.label())
        self._set_connected(True)

    def _on_disconnect(self) -> None:
        self._controller.disconnect()
        self._set_connected(False)

    def _set_connected(self, connected: bool) -> None:
        self._conn_bar.set_connected(connected)
        self._control.set_connected(connected)
        self._map.set_connected(connected)

    # ── periodic refresh ───────────────────────────────────────────────────
    def _tick(self) -> None:
        snap = self._controller.snapshot()
        self._hud.update_from(snap)
        self._control.update_from(snap)
        self._map.update_from(snap)

        for notice in self._controller.drain_notices():
            self._messages.add(notice)
            if MessagesPanel.is_alert(notice):
                self._warnings.push(notice)   # surface it prominently over the map
        self._warnings.prune()

    # ── lifecycle ───────────────────────────────────────────────────────────
    def closeEvent(self, event) -> None:
        self._timer.stop()
        self._controller.disconnect()
        self._config.save()
        super().closeEvent(event)
