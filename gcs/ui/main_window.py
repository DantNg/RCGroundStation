"""Main window — map/camera fly view with floating instrument overlays.

For autonomous work the **primary view is the workspace**: either the satellite
map or the live camera fills the window, while a small circular HUD, the control
panel and the message log float on top and warnings surface over it. The view
that isn't primary shrinks into a corner picture-in-picture tile; tapping it (or
pressing ``V``) swaps the two. Once a link is up the connection bar collapses
into a slim status chip so the view gets the whole window.

This is the only object that knows about :class:`GcsController`; a ~25 Hz timer
polls the store, feeds the passive overlays and drains notices into the log +
on-map warning banners.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, QTimer
from PySide6.QtGui import QKeySequence, QShortcut
from PySide6.QtWidgets import QPushButton, QVBoxLayout, QWidget

from ..app.controller import GcsController
from ..config import AppConfig
from . import acrylic, theme
from .camera_view import CameraView
from .connection_bar import ConnectionBar
from .control_panel import ControlPanel
from .map_widget import MapWidget
from .messages_panel import MessagesPanel
from .overlay_stage import OverlayStage
from .round_hud import RoundHud
from .top_bar import TopBar
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
        self._control_open = False   # control panel starts collapsed (map unobstructed)

        self.setWindowTitle("Lite Ground Station — Desktop")
        self.resize(1320, 820)
        self.setMinimumSize(460, 320)
        self.setStyleSheet(theme.STYLESHEET)

        # frosted-glass bars sample whichever view is currently primary
        acrylic.set_backdrop_provider(lambda: self._view(self._primary))

        root = QVBoxLayout(self)
        root.setContentsMargins(8, 8, 8, 8)
        root.setSpacing(6)

        # connection bar — a frosted card floating over the map until a link is
        # up, when it gives way to the slim top pill (added to the stage below)
        self._conn_bar = ConnectionBar(config)
        self._conn_bar.connect_requested.connect(self._on_connect)
        self._conn_bar.disconnect_requested.connect(self._on_disconnect)

        # ── fly view: primary base (map/camera) + floating overlays ───────────
        self._map = MapWidget()
        self._camera = CameraView()
        self._hud = RoundHud()
        self._control = ControlPanel()
        self._messages = MessagesPanel()
        self._warnings = WarningOverlay()
        self._topbar = TopBar()
        self._topbar.attach_controls(self._map.controls_widget(),
                                     self._camera.header_widget())
        self._pip_frame = PipOverlay()

        # control is collapsed by default; a small handle expands it on demand
        self._control.setVisible(False)
        self._control_toggle = QPushButton("⚙")
        self._control_toggle.setObjectName("IconButton")
        self._control_toggle.setToolTip("Show controls — arm · mode · takeoff (C)")
        self._control_toggle.setCursor(Qt.PointingHandCursor)
        self._control_toggle.clicked.connect(self._toggle_control)

        self._control.arm_requested.connect(self._controller.commands.arm)
        self._control.disarm_requested.connect(self._controller.commands.disarm)
        self._control.mode_requested.connect(self._controller.commands.set_mode_by_name)
        self._control.takeoff_requested.connect(self._controller.commands.takeoff)
        self._map.fly_to_requested.connect(self._controller.commands.fly_to)
        self._topbar.disconnect_requested.connect(self._on_disconnect)
        self._pip_frame.clicked.connect(self._swap_views)

        self._stage = OverlayStage(self._layout_overlays)
        for w in (self._map, self._camera, self._hud, self._control,
                  self._messages, self._warnings, self._topbar, self._pip_frame,
                  self._control_toggle, self._conn_bar):
            self._stage.add(w)
        root.addWidget(self._stage, 1)
        self._apply_view_roles()

        # keyboard: V swaps views, C toggles controls, F11 fullscreen, Esc leaves it
        QShortcut(QKeySequence("V"), self, activated=self._swap_views)
        QShortcut(QKeySequence("C"), self, activated=self._toggle_control)
        QShortcut(QKeySequence("F11"), self, activated=self._toggle_fullscreen)
        QShortcut(QKeySequence("Esc"), self, activated=self._exit_fullscreen)

        # refresh timer
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

        self._topbar.set_active(self._primary)   # show the primary view's controls
        self._pip_frame.set_caption("MAP" if pip is self._map else "CAM")

        # back-to-front: primary, instruments, warnings, pip, pip frame, pill, toggle
        primary.lower()
        for ov in (self._hud, self._control, self._messages, self._warnings):
            ov.raise_()
        pip.raise_()
        self._pip_frame.raise_()
        self._topbar.raise_()
        self._control_toggle.raise_()
        self._conn_bar.raise_()   # the connect card sits above everything

    def _swap_views(self) -> None:
        self._primary = self._pip_name()
        if self._primary == "cam":
            self._camera.ensure_started()
        self._apply_view_roles()
        self._relayout()

    def _toggle_control(self) -> None:
        """Expand/collapse the control panel (it stays hidden so the map is clear)."""
        self._control_open = not self._control_open
        self._control_toggle.setText("✕" if self._control_open else "⚙")
        self._control.setVisible(self._control_open)
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

        # control toggle — a small handle pinned to the top-right corner. The
        # control panel itself stays hidden so it never covers the map; the user
        # taps the handle (or presses C) to slide it out below the handle.
        bw = self._control_toggle.sizeHint().width()
        bh = self._control_toggle.sizeHint().height()
        self._control_toggle.setGeometry(w - bw - m, m, bw, bh)

        cw = 140 if tiny else (168 if small else 198)
        ctrl_w = 0
        if self._control_open:
            self._control.set_compact(small)
            self._control.setFixedWidth(cw)
            ctrl_top = m + bh + m
            ch = min(self._control.sizeHint().height(), h - ctrl_top - m)
            self._control.setGeometry(w - cw - m, ctrl_top, cw, ch)
            ctrl_w = cw
        # what top-edge overlays must keep clear on the right: the open panel,
        # else just the toggle handle.
        right_top = max(ctrl_w, bw)

        # connection card — floats top-centre over the map while disconnected.
        if self._conn_bar.isVisible():
            cb_w = min(self._conn_bar.sizeHint().width(), w - 2 * m)
            cb_h = self._conn_bar.sizeHint().height()
            self._conn_bar.setGeometry((w - cb_w) // 2, m, cb_w, cb_h)

        # combined top pill (connection status + active view's controls) — top-left.
        # On small screens its controls shed their labels and it's capped so it
        # never slides under the right-hand controls.
        self._topbar.set_compact(small)
        self._map.set_controls_compact(small)
        self._camera.set_header_compact(small)
        tb_max = max(180, w - right_top - 3 * m)
        tb_w = min(self._topbar.sizeHint().width(), w - 2 * m, tb_max)
        tb_h = self._topbar.sizeHint().height()
        self._topbar.setGeometry(m, m, tb_w, tb_h)
        tb_right = m + tb_w

        # PIP tile — top-left below the pill; shrinks with the window, never hidden
        pip_x = m
        pip_y = m + tb_h + m
        prop = 0.20 if tiny else (0.22 if small else 0.24)
        pip_w = int(_clamp(w * prop, 92, 300))
        pip_h = int(pip_w * 9 / 16)
        max_pip_h = int(_clamp(h * 0.30, 52, 320))
        if pip_h > max_pip_h:
            pip_h = max_pip_h
            pip_w = int(pip_h * 16 / 9)
        pip.setVisible(True)
        self._pip_frame.setVisible(True)
        pip.setGeometry(pip_x, pip_y, pip_w, pip_h)
        self._pip_frame.setGeometry(pip_x, pip_y, pip_w, pip_h)
        pip_bottom = pip_y + pip_h

        # round HUD — bottom-left, scaled down on small panels and kept clear of
        # the PIP above it (so it shrinks instead of overlapping).
        hud_prop = 0.20 if tiny else (0.24 if small else 0.28)
        hud_floor = 84 if tiny else (110 if small else 150)
        avail_hud = h - m - (pip_bottom + m)
        hud_sz = int(_clamp(min(w * hud_prop, h * 0.40), hud_floor, 240))
        hud_h = hud_sz + 52
        if hud_h > avail_hud:
            hud_h = max(100, avail_hud)
            hud_sz = max(80, min(hud_sz, hud_h - 52))
        self._hud.setGeometry(m, h - hud_h - m, hud_sz, hud_h)

        # message log — bottom strip between HUD and control. Kept on roomy
        # screens; hidden once the window shrinks (small) so the view stays clear.
        msg_left = m + hud_sz + m
        msg_right = w - ctrl_w - 2 * m
        msg_w = msg_right - msg_left
        msg_h = int(_clamp(h * 0.22, 90, 150))
        if msg_w >= 240 and not small:
            self._messages.setGeometry(msg_left, h - msg_h - m, msg_w, msg_h)
            self._messages.setVisible(True)
        else:
            self._messages.setVisible(False)

        # warning banners — top band, centred between the top pill and the right controls
        warn_l = max(tb_right, pip_x + pip_w) + m
        warn_r = w - right_top - 2 * m
        avail = max(0, warn_r - warn_l)
        warn_w = int(_clamp(min(avail, 600), 200, 600)) if avail > 200 else max(avail, 120)
        warn_x = warn_l + max(0, (avail - warn_w) // 2)
        self._warnings.setGeometry(warn_x, m, warn_w, int(_clamp(h * 0.4, 120, 200)))

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
        self._conn_bar.setVisible(not connected)   # collapse the bar → top pill
        self._topbar.set_connected(connected)
        if connected:
            self._topbar.set_state(False, self._config.label())
        self._control.set_connected(connected)
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
        self._control.update_from(snap)
        self._map.update_from(snap)
        if self._connected:
            self._topbar.set_state(snap.link.link_up, self._config.label())

        for notice in self._controller.drain_notices():
            self._messages.add(notice)
            if MessagesPanel.is_alert(notice):
                self._warnings.push(notice)   # surface it prominently over the view
        self._warnings.prune()

    # ── lifecycle ───────────────────────────────────────────────────────────
    def closeEvent(self, event) -> None:
        self._timer.stop()
        self._camera.shutdown()
        self._controller.disconnect()
        self._config.save()
        super().closeEvent(event)
