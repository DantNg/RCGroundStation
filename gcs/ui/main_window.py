"""Main window — map/camera fly view with a NASA-style instrument layout.

The workspace (satellite map or live camera) fills the window. Floating over it:
a **full-width status bar** at the top (vehicle · battery · GPS · link · mode ·
arm/disarm), a **left action rail** of one-tap flight modes (LAND · RETURN ·
PAUSE · ACTION), a small **round attitude HUD** (IMU + altitude), the camera
**picture-in-picture** tile, the message log, on-map warning banners, and a
corner **dock** with the active view's own controls (map provider/3D/Plan/zoom or
camera device/start). While disconnected the connection card floats as a centred
hero so the top controls stay clear of it.

This is the only object that knows about :class:`GcsController`; a ~25 Hz timer
polls the store, feeds the passive overlays and drains notices into the log +
on-map warning banners.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QKeySequence, QShortcut
from PySide6.QtWidgets import QInputDialog, QMessageBox, QVBoxLayout, QWidget

from ..app.controller import GcsController
from ..config import AppConfig
from . import acrylic, theme
from .camera_view import CameraView
from .connection_bar import ConnectionBar
from .map_widget import MapWidget
from .messages_panel import MessagesPanel
from .mode_rail import ModeRail
from .overlay_stage import OverlayStage
from .round_hud import RoundHud
from .top_bar import ControlDock, StatusBar
from .warning_overlay import WarningOverlay
from .widgets import PipOverlay

_REFRESH_HZ = 25


def _clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


class MainWindow(QWidget):
    def __init__(self, controller: GcsController, config: AppConfig):
        super().__init__()
        self._controller = controller
        self._config = config
        self._connected = False
        self._primary = "map"   # "map" | "cam"
        self._last_takeoff_alt = 10.0

        self.setWindowTitle("Lite Ground Station — Desktop")
        self.resize(1280, 720)
        self.setMinimumSize(460, 320)
        self.setStyleSheet(theme.STYLESHEET)

        # frosted-glass bars sample whichever view is currently primary
        acrylic.set_backdrop_provider(lambda: self._view(self._primary))

        root = QVBoxLayout(self)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(6)

        # connection card — a frosted hero floating over the map until a link is up
        self._conn_bar = ConnectionBar(config)
        self._conn_bar.connect_requested.connect(self._on_connect)
        self._conn_bar.disconnect_requested.connect(self._on_disconnect)

        # ── fly view: primary base (map/camera) + floating overlays ───────────
        self._map = MapWidget()
        self._camera = CameraView()
        self._hud = RoundHud()
        self._messages = MessagesPanel()
        self._warnings = WarningOverlay()
        self._status = StatusBar()
        self._rail = ModeRail()
        self._dock = ControlDock()
        self._dock.attach_controls(self._map.controls_widget(),
                                   self._camera.header_widget())
        self._pip_frame = PipOverlay()

        # wiring: status bar + rail announce intent; the controller executes it
        self._status.arm_requested.connect(self._on_arm)
        self._status.disarm_requested.connect(self._controller.commands.disarm)
        self._status.mode_requested.connect(self._controller.commands.set_mode_by_name)
        self._status.disconnect_requested.connect(self._on_disconnect)
        self._rail.mode_requested.connect(self._controller.commands.set_mode_by_name)
        self._rail.takeoff_requested.connect(self._on_takeoff)
        self._map.fly_to_requested.connect(self._controller.commands.fly_to)
        self._map.mission_upload_requested.connect(self._controller.mission.upload)
        self._map.mission_start_requested.connect(self._on_start_mission)
        self._pip_frame.clicked.connect(self._swap_views)

        self._stage = OverlayStage(self._layout_overlays)
        for w in (self._map, self._camera, self._hud, self._messages,
                  self._warnings, self._status, self._rail, self._dock,
                  self._pip_frame, self._conn_bar):
            self._stage.add(w)
        root.addWidget(self._stage, 1)
        self._apply_view_roles()

        # keyboard: V swaps views, F11 fullscreen, Esc leaves it
        QShortcut(QKeySequence("V"), self, activated=self._swap_views)
        QShortcut(QKeySequence("F11"), self, activated=self._toggle_fullscreen)
        QShortcut(QKeySequence("Esc"), self, activated=self._exit_fullscreen)

        self._timer = QTimer(self)
        self._timer.timeout.connect(self._tick)
        self._timer.start(int(1000 / _REFRESH_HZ))

    # ── view roles / swapping ─────────────────────────────────────────────────
    def _view(self, name: str) -> QWidget:
        return self._map if name == "map" else self._camera

    def _pip_name(self) -> str:
        return "cam" if self._primary == "map" else "map"

    def _apply_view_roles(self) -> None:
        """Set chrome + z-order so the primary fills and the other is the PIP."""
        primary = self._view(self._primary)
        pip = self._view(self._pip_name())

        self._dock.set_active(self._primary)   # show the primary view's controls
        self._pip_frame.set_caption("MAP" if pip is self._map else "CAM")

        primary.lower()
        for ov in (self._hud, self._messages, self._warnings, self._rail, self._dock):
            ov.raise_()
        pip.raise_()
        self._pip_frame.raise_()
        self._status.raise_()
        self._conn_bar.raise_()   # the connect card sits above everything

    def _swap_views(self) -> None:
        self._primary = self._pip_name()
        if self._primary == "cam":
            self._camera.ensure_started()
        self._apply_view_roles()
        self._relayout()

    # ── overlay placement (called from the stage's resizeEvent) ───────────────
    def _relayout(self) -> None:
        self._layout_overlays(self._stage.width(), self._stage.height())

    def _layout_overlays(self, w: int, h: int) -> None:
        m = 8
        tiny = (w < 560 or h < 380)
        small = (w < 860 or h < 520)

        primary = self._view(self._primary)
        pip = self._view(self._pip_name())
        primary.setGeometry(0, 0, w, h)
        primary.setVisible(True)

        # ── full-width status bar across the top ─────────────────────────────
        self._status.set_compact(small)
        sb_h = self._status.sizeHint().height()
        self._status.setGeometry(m, m, w - 2 * m, sb_h)
        content_top = m + sb_h + m

        # ── left action rail (below the status bar, top-aligned) ─────────────
        rail_w = self._rail.sizeHint().width()
        rail_h = min(self._rail.sizeHint().height(), h - content_top - m)
        self._rail.setGeometry(m, content_top, rail_w, rail_h)
        rail_right = m + rail_w
        rail_bottom = content_top + rail_h

        # ── camera PIP — top-right, below the status bar ─────────────────────
        prop = 0.20 if tiny else (0.22 if small else 0.24)
        pip_w = int(_clamp(w * prop, 92, 300))
        pip_h = int(pip_w * 9 / 16)
        max_pip_h = int(_clamp(h * 0.30, 52, 320))
        if pip_h > max_pip_h:
            pip_h = max_pip_h
            pip_w = int(pip_h * 16 / 9)
        pip_x = w - pip_w - m
        pip_y = content_top
        pip.setVisible(True)
        self._pip_frame.setVisible(True)
        pip.setGeometry(pip_x, pip_y, pip_w, pip_h)
        self._pip_frame.setGeometry(pip_x, pip_y, pip_w, pip_h)

        # ── map/camera controls dock — bottom-right corner ───────────────────
        self._map.set_controls_compact(small)
        self._camera.set_header_compact(small)
        dock_w = min(self._dock.sizeHint().width(), w - rail_right - 2 * m)
        dock_h = self._dock.sizeHint().height()
        dock_x = w - dock_w - m
        dock_y = h - dock_h - m
        self._dock.setGeometry(dock_x, dock_y, dock_w, dock_h)

        # ── round attitude HUD — bottom-left, kept clear of the rail above ───
        hud_sz = int(_clamp(min(w * 0.22, h * 0.34), 84 if tiny else 110, 220))
        hud_h = hud_sz + int(28)
        avail_hud = h - m - (rail_bottom + m)
        if hud_h > avail_hud:
            hud_h = max(96, avail_hud)
            hud_sz = max(72, min(hud_sz, hud_h - 28))
        self._hud.setGeometry(m, h - hud_h - m, hud_sz, hud_h)

        # ── message log — bottom strip between the HUD and the dock ──────────
        msg_left = m + hud_sz + m
        msg_right = dock_x - m
        msg_w = msg_right - msg_left
        msg_h = int(_clamp(h * 0.22, 90, 150))
        if msg_w >= 240 and not small:
            self._messages.setGeometry(msg_left, h - msg_h - m, msg_w, msg_h)
            self._messages.setVisible(True)
        else:
            self._messages.setVisible(False)

        # ── warning banners — top band between the rail and the PIP ──────────
        warn_l = rail_right + m
        warn_r = pip_x - m
        avail = max(0, warn_r - warn_l)
        warn_w = int(_clamp(min(avail, 600), 200, 600)) if avail > 200 else max(avail, 120)
        warn_x = warn_l + max(0, (avail - warn_w) // 2)
        self._warnings.setGeometry(warn_x, content_top, warn_w,
                                   int(_clamp(h * 0.4, 120, 200)))

        # ── connection card — centred hero while disconnected ────────────────
        if not self._connected:
            cb_w = min(self._conn_bar.sizeHint().width(), w - 2 * m)
            cb_h = self._conn_bar.sizeHint().height()
            cb_y = max(content_top, (h - cb_h) // 2)
            self._conn_bar.setGeometry((w - cb_w) // 2, cb_y, cb_w, cb_h)

    # ── commands (with confirmations) ────────────────────────────────────────
    def _on_arm(self, force: bool) -> None:
        if self._confirm("Arm the vehicle?",
                         "The motors may spin. Make sure the area is clear."):
            self._controller.commands.arm(force)

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
        self._controller.commands.takeoff(alt)

    def _on_start_mission(self) -> None:
        if self._confirm("Start mission?",
                         "The vehicle will switch to AUTO and fly the uploaded "
                         "mission. Make sure it is armed and the area is clear."):
            self._controller.commands.start_mission()

    def _confirm(self, title: str, text: str) -> bool:
        box = QMessageBox(self)
        box.setIcon(QMessageBox.Warning)
        box.setWindowTitle(title)
        box.setText(text)
        box.setStandardButtons(QMessageBox.Yes | QMessageBox.No)
        box.setDefaultButton(QMessageBox.No)
        return box.exec() == QMessageBox.Yes

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
        self._connected = connected
        self._conn_bar.set_connected(connected)
        self._conn_bar.setVisible(not connected)
        self._status.set_connected(connected)
        if connected:
            self._status.set_label(self._config.label())
        self._rail.set_connected(connected)
        self._map.set_connected(connected)
        self._relayout()

    def _toggle_fullscreen(self) -> None:
        if self.isFullScreen():
            self.showNormal()
        else:
            self.showFullScreen()

    def _exit_fullscreen(self) -> None:
        if self.isFullScreen():
            self.showNormal()

    # ── periodic refresh ───────────────────────────────────────────────────
    def _tick(self) -> None:
        snap = self._controller.snapshot()
        self._hud.update_from(snap)
        self._map.update_from(snap)
        if self._connected:
            self._status.update_from(snap)
            self._rail.update_from(snap)

        for notice in self._controller.drain_notices():
            self._messages.add(notice)
            if MessagesPanel.is_alert(notice):
                self._warnings.push(notice)
        self._warnings.prune()

    # ── lifecycle ───────────────────────────────────────────────────────────
    def closeEvent(self, event) -> None:
        self._timer.stop()
        self._camera.shutdown()
        self._controller.disconnect()
        self._config.save()
        super().closeEvent(event)
