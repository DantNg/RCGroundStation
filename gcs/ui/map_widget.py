"""Slippy-map view with the drone marker, a waypoint mission and a 2D/3D toggle.

The 2D map is a hand-rolled Web-Mercator XYZ tile renderer: tiles are fetched on
a background thread, cached to disk and in memory, and drawn under a
heading-rotated drone marker plus a breadcrumb trail. On top of that it hosts a
**waypoint mission** (add / drag / delete points, each with its own altitude) and
a **local flight simulator** that flies a marker along the route at a chosen
groundspeed — no vehicle required.

Switching to **3D** swaps the QPainter canvas for a Cesium globe
(:class:`~gcs.ui.cesium_view.CesiumView`) that renders the *same* mission as 3D
waypoints with altitude poles and animates the same simulation. The two views
share one :class:`~gcs.domain.mission.Mission`, so an edit in either is reflected
in both.
"""
from __future__ import annotations

import math
import queue
import threading
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Set, Tuple

from PySide6.QtCore import QObject, QPointF, Qt, QTimer, Signal
from PySide6.QtGui import (QBrush, QColor, QFont, QImage, QPainter, QPen,
                           QPolygonF)
from PySide6.QtWidgets import (QComboBox, QHBoxLayout, QInputDialog, QLabel,
                               QMenu, QPushButton, QStackedWidget, QVBoxLayout,
                               QWidget, QWidgetAction)

from ..domain.mission import Mission
from ..domain.telemetry import TelemetrySnapshot
from . import theme
from .waypoint_editor import WaypointEditor

TILE = 256
_CACHE_DIR = Path.home() / ".lite_gcs" / "tiles"
_USER_AGENT = "LiteGCS-Desktop/1.0 (+https://github.com/)"
_SIM_HZ = 30


@dataclass(frozen=True)
class TileProvider:
    name: str
    url_template: str   # {z}/{x}/{y}
    max_zoom: int = 19

    def url(self, z: int, x: int, y: int) -> str:
        return self.url_template.format(z=z, x=x, y=y)


PROVIDERS = {
    "Satellite": TileProvider(
        "Satellite",
        "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/"
        "MapServer/tile/{z}/{y}/{x}", max_zoom=19),
    "Street": TileProvider(
        "Street", "https://tile.openstreetmap.org/{z}/{x}/{y}.png", max_zoom=19),
}


def _lonlat_to_world_px(lat: float, lon: float, z: int) -> Tuple[float, float]:
    n = 2 ** z
    lat = max(min(lat, 85.05112878), -85.05112878)
    lat_rad = math.radians(lat)
    x = (lon + 180.0) / 360.0 * n
    y = (1.0 - math.asinh(math.tan(lat_rad)) / math.pi) / 2.0 * n
    return x * TILE, y * TILE


class _TileLoader(QObject):
    """Background tile downloader. Emits ``ready`` when a new tile is cached."""

    ready = Signal()

    def __init__(self):
        super().__init__()
        self._mem: Dict[Tuple[str, int, int, int], QImage] = {}
        self._pending: Set[Tuple[str, int, int, int]] = set()
        self._failed: Set[Tuple[str, int, int, int]] = set()
        self._lock = threading.Lock()
        self._q: "queue.Queue" = queue.Queue()
        self._thread = threading.Thread(target=self._run, name="tile-loader", daemon=True)
        self._thread.start()

    def get(self, provider: TileProvider, z: int, x: int, y: int) -> Optional[QImage]:
        key = (provider.name, z, x, y)
        with self._lock:
            if key in self._mem:
                return self._mem[key]
            if key in self._failed:
                return None
            disk = self._disk_path(key)
            if disk.exists():
                img = QImage(str(disk))
                if not img.isNull():
                    self._mem[key] = img
                    return img
            if key not in self._pending:
                self._pending.add(key)
                self._q.put((provider, key))
        return None

    def _run(self) -> None:
        while True:
            provider, key = self._q.get()
            _name, z, x, y = key
            try:
                data = self._download(provider.url(z, x, y))
                path = self._disk_path(key)
                path.parent.mkdir(parents=True, exist_ok=True)
                path.write_bytes(data)
                img = QImage()
                img.loadFromData(data)
                with self._lock:
                    self._pending.discard(key)
                    if not img.isNull():
                        self._mem[key] = img
                self.ready.emit()
            except Exception:
                with self._lock:
                    self._pending.discard(key)
                    self._failed.add(key)

    @staticmethod
    def _download(url: str) -> bytes:
        req = urllib.request.Request(url, headers={"User-Agent": _USER_AGENT})
        with urllib.request.urlopen(req, timeout=6) as resp:
            return resp.read()

    @staticmethod
    def _disk_path(key) -> Path:
        name, z, x, y = key
        return _CACHE_DIR / name / str(z) / str(x) / f"{y}.png"


class MapWidget(QWidget):
    # lat, lon, altitude(m) — emitted on "Fly To Here"
    fly_to_requested = Signal(float, float, float)
    mission_upload_requested = Signal(list)   # [{"lat","lon","alt"}, …]
    mission_start_requested = Signal()         # run the uploaded mission (AUTO)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._provider = PROVIDERS["Satellite"]
        self._zoom = 16
        self._lat = 21.0285
        self._lon = 105.8048
        self._alt_rel = 0.0
        self._have_fix = False
        self._heading = 0.0
        self._follow = True
        self._connected = False
        self._guided_alt = 30.0
        self._target: Optional[Tuple[float, float]] = None
        self._center_lat = self._lat
        self._center_lon = self._lon
        self._trail: list[Tuple[float, float]] = []
        self._drag_last: Optional[QPointF] = None

        # ── mission + 3D + simulation state ──────────────────────────────────
        self._mission = Mission()
        self._edit = False
        self._mode_3d = False
        self._cesium = None          # lazily created on first switch to 3D
        self._drag_wp: Optional[int] = None
        self._sim_active = False
        self._sim_dist = 0.0
        self._sim_speed = 10.0       # m/s groundspeed for the preview
        self._sim_pos: Optional[Tuple[float, float, float, float]] = None
        self._sim_timer = QTimer(self)
        self._sim_timer.timeout.connect(self._sim_step)

        self._loader = _TileLoader()
        self._loader.ready.connect(self._on_tile_ready)

        # The map canvas fills the whole widget; its controls live in a separate
        # compact bar that the MainWindow hosts in the shared top pill.
        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)
        self._controls_bar = self._build_controls()
        self._canvas = _MapCanvas(self)
        self._stack = QStackedWidget(self)
        self._stack.addWidget(self._canvas)   # index 0 = 2D
        outer.addWidget(self._stack, 1)

    def _build_controls(self) -> QWidget:
        bar = QWidget()
        bar.setAttribute(Qt.WA_TranslucentBackground, True)
        row = QHBoxLayout(bar)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(6)
        self._map_label = QLabel("Map:")
        row.addWidget(self._map_label)
        combo = QComboBox()
        combo.addItems(list(PROVIDERS.keys()))
        combo.setMaximumWidth(112)
        combo.currentTextChanged.connect(self._set_provider)
        row.addWidget(combo)

        self._mode3d_btn = QPushButton("3D")
        self._mode3d_btn.setCheckable(True)
        self._mode3d_btn.setToolTip("Switch between the 2D map and the 3D globe")
        self._mode3d_btn.clicked.connect(self._toggle_3d)
        row.addWidget(self._mode3d_btn)

        self._follow_btn = QPushButton("Follow")
        self._follow_btn.setCheckable(True)
        self._follow_btn.setChecked(True)
        self._follow_btn.setToolTip("Keep the view centred on the vehicle")
        self._follow_btn.clicked.connect(self._toggle_follow)
        row.addWidget(self._follow_btn)

        # mission + simulation tools live behind one "Plan" button so the always-
        # visible strip stays slim and never collides with the connection card
        self._plan_btn = QPushButton("Plan")
        self._plan_btn.setToolTip("Waypoint mission & flight simulation")
        self._plan_menu = QMenu(self._plan_btn)

        self._edit_action = self._plan_menu.addAction("✎  Edit waypoints")
        self._edit_action.setCheckable(True)
        self._edit_action.setToolTip("Tap empty map to add a waypoint, tap a "
                                     "waypoint to edit its altitude or delete it, "
                                     "drag to move")
        self._edit_action.toggled.connect(self._on_edit_toggled)

        self._clear_action = self._plan_menu.addAction("🗑  Clear mission")
        self._clear_action.triggered.connect(self._clear_mission)

        self._plan_menu.addSeparator()

        self._sim_action = self._plan_menu.addAction("▶  Simulate route")
        self._sim_action.setCheckable(True)
        self._sim_action.setToolTip("Fly a marker along the route (visual preview only)")
        self._sim_action.toggled.connect(self._on_sim_toggled)

        speed_w = QWidget()
        speed_row = QHBoxLayout(speed_w)
        speed_row.setContentsMargins(24, 2, 10, 4)
        speed_row.setSpacing(6)
        speed_row.addWidget(QLabel("Speed"))
        self._speed_combo = QComboBox()
        self._speed_combo.addItems(["5 m/s", "10 m/s", "20 m/s", "40 m/s"])
        self._speed_combo.setCurrentText("10 m/s")
        self._speed_combo.currentTextChanged.connect(self._set_sim_speed)
        speed_row.addWidget(self._speed_combo, 1)
        speed_action = QWidgetAction(self._plan_menu)
        speed_action.setDefaultWidget(speed_w)
        self._plan_menu.addAction(speed_action)

        self._plan_menu.addSeparator()
        self._upload_action = self._plan_menu.addAction("⬆  Upload to vehicle")
        self._upload_action.setToolTip("Send the planned waypoints to the vehicle "
                                       "as an AUTO mission")
        self._upload_action.triggered.connect(self._on_upload)
        self._start_action = self._plan_menu.addAction("➤  Start mission (AUTO)")
        self._start_action.setToolTip("Switch the vehicle to AUTO and run the "
                                      "uploaded mission")
        self._start_action.triggered.connect(self.mission_start_requested.emit)
        self._upload_action.setEnabled(False)   # enabled once a vehicle is connected
        self._start_action.setEnabled(False)

        self._plan_btn.setMenu(self._plan_menu)
        row.addWidget(self._plan_btn)

        minus = QPushButton("−")
        minus.setObjectName("IconButton")
        minus.clicked.connect(lambda: self._set_zoom(self._zoom - 1))
        plus = QPushButton("+")
        plus.setObjectName("IconButton")
        plus.clicked.connect(lambda: self._set_zoom(self._zoom + 1))
        row.addWidget(minus)
        row.addWidget(plus)
        return bar

    def controls_widget(self) -> QWidget:
        """The map's provider/3D/follow/edit/sim/zoom bar (hosted in the top pill)."""
        return self._controls_bar

    def set_controls_compact(self, compact: bool) -> None:
        # the mission/sim/speed controls already live in the "Plan" pop-up menu,
        # so the strip just drops the "Map:" caption when space is tight
        self._map_label.setVisible(not compact)

    # ── data in ────────────────────────────────────────────────────────────
    def update_from(self, s: TelemetrySnapshot) -> None:
        if s.position.valid:
            self._lat = s.position.lat
            self._lon = s.position.lon
            self._alt_rel = s.position.alt_rel
            self._heading = s.position.heading_deg
            if not self._have_fix:
                self._have_fix = True
                self._center_lat, self._center_lon = self._lat, self._lon
            if not self._trail or self._dist(self._trail[-1], (self._lat, self._lon)) > 1e-6:
                self._trail.append((self._lat, self._lon))
                if len(self._trail) > 400:
                    self._trail.pop(0)
            if self._follow:
                self._center_lat, self._center_lon = self._lat, self._lon
        if self._cesium is not None:
            self._cesium.set_vehicle(self._lat, self._lon, self._alt_rel,
                                     self._heading, self._have_fix)
        self._canvas.update()

    @staticmethod
    def _dist(a, b) -> float:
        return abs(a[0] - b[0]) + abs(a[1] - b[1])

    # ── controls ────────────────────────────────────────────────────────────
    def _set_provider(self, name: str) -> None:
        self._provider = PROVIDERS.get(name, self._provider)
        self._canvas.update()

    def _set_zoom(self, z: int) -> None:
        self._zoom = max(2, min(self._provider.max_zoom, z))
        self._canvas.update()

    def _toggle_follow(self) -> None:
        self._follow = self._follow_btn.isChecked()
        if self._follow and self._have_fix:
            self._center_lat, self._center_lon = self._lat, self._lon
        if self._cesium is not None:
            self._cesium.set_follow(self._follow)
        self._canvas.update()

    def _toggle_3d(self) -> None:
        self._mode_3d = self._mode3d_btn.isChecked()
        if self._mode_3d and self._cesium is None:
            from .cesium_view import CesiumView   # lazy: pull in WebEngine on demand
            self._cesium = CesiumView()
            self._cesium.waypoint_added.connect(self._on_wp_added)
            self._cesium.waypoint_moved.connect(self._on_wp_moved)
            self._cesium.waypoint_removed.connect(self._on_wp_removed)
            self._cesium.waypoint_alt_changed.connect(self._on_wp_alt)
            self._cesium.waypoint_clicked.connect(self._edit_waypoint)
            self._stack.addWidget(self._cesium)
            self._cesium.set_edit_mode(self._edit)
            self._cesium.set_follow(self._follow)
            self._cesium.set_vehicle(self._lat, self._lon, self._alt_rel,
                                     self._heading, self._have_fix)
            self._push_mission()
        self._stack.setCurrentWidget(self._cesium if self._mode_3d else self._canvas)

    # ── mission editing ───────────────────────────────────────────────────────
    def _on_edit_toggled(self, on: bool) -> None:
        self._edit = on
        if self._cesium is not None:
            self._cesium.set_edit_mode(self._edit)
        self._canvas.update()

    def _clear_mission(self) -> None:
        self._stop_sim()
        self._mission.clear()
        self._push_mission()

    def _on_wp_added(self, lat: float, lon: float) -> None:
        self._mission.add(lat, lon)
        self._push_mission()

    def _on_wp_moved(self, idx: int, lat: float, lon: float) -> None:
        self._mission.move(idx, lat, lon)
        self._push_mission()

    def _on_wp_removed(self, idx: int) -> None:
        self._mission.remove(idx)
        self._push_mission()

    def _on_wp_alt(self, idx: int, delta: float) -> None:
        if 0 <= idx < len(self._mission):
            new_alt = max(0.0, self._mission.waypoints[idx].alt + delta)
            self._mission.set_alt(idx, new_alt)
            self._push_mission()

    def _edit_waypoint(self, idx: int) -> None:
        """Tap a waypoint → a touch-friendly popup to set its altitude or delete it."""
        if not (0 <= idx < len(self._mission)):
            return
        dlg = WaypointEditor(idx, self._mission.waypoints[idx].alt, self)
        dlg.alt_changed.connect(lambda a, i=idx: self._set_wp_alt(i, a))
        dlg.delete_requested.connect(lambda i=idx: self._on_wp_removed(i))
        dlg.exec()

    def _set_wp_alt(self, idx: int, alt: float) -> None:
        self._mission.set_alt(idx, max(0.0, alt))
        self._push_mission()

    def _push_mission(self) -> None:
        """Re-render the mission in both views after any edit."""
        self._canvas.update()
        if self._cesium is not None:
            self._cesium.set_waypoints(self._mission.as_list())

    # ── simulation (local visual preview) ──────────────────────────────────────
    def _set_sim_speed(self, text: str) -> None:
        try:
            self._sim_speed = float(text.split()[0])
        except (ValueError, IndexError):
            pass

    def _on_sim_toggled(self, on: bool) -> None:
        if on:
            self._start_sim()
        else:
            self._stop_sim()

    def _start_sim(self) -> None:
        if len(self._mission) < 2:
            self._sim_action.setChecked(False)   # nothing to fly
            return
        self._sim_active = True
        self._sim_dist = 0.0
        self._sim_action.setText("■  Stop simulation")
        self._sim_timer.start(int(1000 / _SIM_HZ))

    def _stop_sim(self) -> None:
        self._sim_active = False
        self._sim_timer.stop()
        self._sim_pos = None
        self._sim_action.setChecked(False)
        self._sim_action.setText("▶  Simulate route")
        if self._cesium is not None:
            self._cesium.clear_sim()
        self._canvas.update()

    def _sim_step(self) -> None:
        total = self._mission.total_length_m()
        self._sim_dist += self._sim_speed * (1.0 / _SIM_HZ)
        pos = self._mission.interpolate(self._sim_dist)
        if pos is None:
            self._stop_sim()
            return
        self._sim_pos = pos
        lat, lon, alt, hdg = pos
        if self._cesium is not None:
            self._cesium.set_sim(lat, lon, alt, hdg)
        self._canvas.update()
        if self._sim_dist >= total:
            self._stop_sim()

    def _on_upload(self) -> None:
        if len(self._mission) < 1:
            return
        self.mission_upload_requested.emit(self._mission.as_list())

    # ── guided interaction (right-click on the map) ──────────────────────────
    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        self._upload_action.setEnabled(connected)
        self._start_action.setEnabled(connected)

    def fly_to_here(self, lat: float, lon: float) -> None:
        self._target = (lat, lon)
        self.fly_to_requested.emit(lat, lon, self._guided_alt)
        self._canvas.update()

    def ask_guided_alt(self) -> None:
        alt, ok = QInputDialog.getDouble(
            self, "Guided altitude", "Fly-to altitude (m above home):",
            self._guided_alt, 1.0, 1000.0, 1)
        if ok:
            self._guided_alt = alt

    def ask_waypoint_alt(self, idx: int) -> None:
        if not (0 <= idx < len(self._mission)):
            return
        cur = self._mission.waypoints[idx].alt
        alt, ok = QInputDialog.getDouble(
            self, "Waypoint altitude", f"Altitude for waypoint {idx + 1} (m):",
            cur, 1.0, 1000.0, 1)
        if ok:
            self._mission.set_alt(idx, alt)
            self._push_mission()

    def clear_target(self) -> None:
        self._target = None
        self._canvas.update()

    def _on_tile_ready(self) -> None:  # pragma: no cover - signal slot
        self._canvas.update()


class _MapCanvas(QWidget):
    """The actual tile/marker painter (kept separate from the control row)."""

    def __init__(self, owner: MapWidget):
        super().__init__(owner)
        self._o = owner
        self.setMinimumSize(320, 240)
        self.setMouseTracking(True)
        self._press: Optional[QPointF] = None
        self._moved = False

    def paintEvent(self, _e) -> None:
        o = self._o
        p = QPainter(self)
        p.fillRect(self.rect(), QColor("#0a0d12"))
        w, h = self.width(), self.height()
        z = o._zoom
        cx_px, cy_px = _lonlat_to_world_px(o._center_lat, o._center_lon, z)
        origin_x = cx_px - w / 2.0
        origin_y = cy_px - h / 2.0

        # visible tile range
        n = 2 ** z
        tx0 = int(math.floor(origin_x / TILE))
        ty0 = int(math.floor(origin_y / TILE))
        tx1 = int(math.floor((origin_x + w) / TILE))
        ty1 = int(math.floor((origin_y + h) / TILE))
        for ty in range(ty0, ty1 + 1):
            if ty < 0 or ty >= n:
                continue
            for tx in range(tx0, tx1 + 1):
                wx = tx % n
                sx = tx * TILE - origin_x
                sy = ty * TILE - origin_y
                img = o._loader.get(o._provider, z, wx, ty)
                if img is not None and not img.isNull():
                    p.drawImage(QPointF(sx, sy), img)
                else:
                    p.fillRect(int(sx), int(sy), TILE, TILE, QColor("#1b2230"))
                    p.setPen(QPen(QColor("#222b3a")))
                    p.drawRect(int(sx), int(sy), TILE, TILE)

        self._draw_trail(p, origin_x, origin_y, z)
        self._draw_mission(p, origin_x, origin_y, z)
        self._draw_target(p, origin_x, origin_y, z)
        self._draw_marker(p, origin_x, origin_y, z)
        self._draw_sim(p, origin_x, origin_y, z)
        self._draw_hud_text(p)
        p.end()

    def _draw_trail(self, p, ox, oy, z) -> None:
        o = self._o
        if len(o._trail) < 2:
            return
        p.setPen(QPen(QColor("#ffd24d"), 2))
        prev = None
        for lat, lon in o._trail:
            px, py = _lonlat_to_world_px(lat, lon, z)
            pt = QPointF(px - ox, py - oy)
            if prev is not None:
                p.drawLine(prev, pt)
            prev = pt

    def _draw_mission(self, p, ox, oy, z) -> None:
        o = self._o
        wps = o._mission.waypoints
        if not wps:
            return
        pts = [QPointF(*[c - off for c, off in
                         zip(_lonlat_to_world_px(wp.lat, wp.lon, z), (ox, oy))])
               for wp in wps]
        # planned route
        if len(pts) >= 2:
            p.setPen(QPen(QColor("#4c9bff"), 2, Qt.DashLine))
            for a, b in zip(pts, pts[1:]):
                p.drawLine(a, b)
        # numbered waypoint dots with altitude labels
        p.setFont(QFont("Consolas", 8))
        for i, (pt, wp) in enumerate(zip(pts, wps)):
            p.setPen(QPen(QColor("#0d1117"), 2))
            p.setBrush(QBrush(QColor("#ffd24d")))
            p.drawEllipse(pt, 7, 7)
            p.setPen(QPen(QColor("#0d1117")))
            p.drawText(pt + QPointF(-3, 4), str(i + 1))
            p.setPen(QPen(QColor("#cfe3ff")))
            p.drawText(pt + QPointF(10, 4), f"{wp.alt:.0f}m")

    def _draw_target(self, p, ox, oy, z) -> None:
        o = self._o
        if o._target is None:
            return
        lat, lon = o._target
        px, py = _lonlat_to_world_px(lat, lon, z)
        sx, sy = px - ox, py - oy
        p.setBrush(Qt.NoBrush)
        p.setPen(QPen(QColor("#ff3df0"), 2))
        p.drawEllipse(QPointF(sx, sy), 9, 9)
        p.drawLine(QPointF(sx - 13, sy), QPointF(sx + 13, sy))
        p.drawLine(QPointF(sx, sy - 13), QPointF(sx, sy + 13))

    def _draw_marker(self, p, ox, oy, z) -> None:
        o = self._o
        if not o._have_fix:
            return
        px, py = _lonlat_to_world_px(o._lat, o._lon, z)
        sx, sy = px - ox, py - oy
        p.save()
        p.translate(sx, sy)
        p.rotate(o._heading)
        p.setPen(QPen(QColor("#0d1117"), 2))
        p.setBrush(QBrush(QColor(theme.ACCENT)))
        p.drawPolygon(QPolygonF([
            QPointF(0, -12), QPointF(8, 9), QPointF(0, 4), QPointF(-8, 9)]))
        p.restore()

    def _draw_sim(self, p, ox, oy, z) -> None:
        o = self._o
        if o._sim_pos is None:
            return
        lat, lon, alt, hdg = o._sim_pos
        px, py = _lonlat_to_world_px(lat, lon, z)
        sx, sy = px - ox, py - oy
        p.save()
        p.translate(sx, sy)
        p.rotate(hdg)
        p.setPen(QPen(QColor("#0d1117"), 2))
        p.setBrush(QBrush(QColor("#3fb950")))
        p.drawPolygon(QPolygonF([
            QPointF(0, -12), QPointF(8, 9), QPointF(0, 4), QPointF(-8, 9)]))
        p.restore()
        p.setFont(QFont("Consolas", 8))
        p.setPen(QPen(QColor("#3fb950")))
        p.drawText(QPointF(sx + 12, sy - 6), f"SIM {alt:.0f}m")

    def _draw_hud_text(self, p) -> None:
        o = self._o
        p.setFont(QFont("Consolas", 9))
        if o._have_fix:
            txt = f"{o._lat:.6f}, {o._lon:.6f}   z{o._zoom}"
        else:
            txt = f"no GPS fix   z{o._zoom}"
        if o._mission.waypoints:
            txt += f"   WP{len(o._mission)} · {o._mission.total_length_m():.0f}m"
        # bottom-right, clear of the floating round HUD (bottom-left)
        tw = len(txt) * 7 + 10
        x = self.width() - tw - 6
        p.fillRect(x, self.height() - 24, tw, 18, QColor(0, 0, 0, 140))
        p.setPen(QPen(QColor(theme.TEXT)))
        p.drawText(x + 4, self.height() - 10, txt)

    # ── right-click menu (guided fly-to, or waypoint editing) ─────────────────
    def contextMenuEvent(self, e) -> None:
        o = self._o
        if o._edit:
            idx = self._wp_at(e.position() if hasattr(e, "position") else QPointF(e.pos()))
            menu = QMenu(self)
            set_alt = menu.addAction("Set waypoint altitude…")
            delete = menu.addAction("Delete waypoint")
            set_alt.setEnabled(idx is not None)
            delete.setEnabled(idx is not None)
            chosen = menu.exec(e.globalPos())
            if idx is not None and chosen is set_alt:
                o.ask_waypoint_alt(idx)
            elif idx is not None and chosen is delete:
                o._on_wp_removed(idx)
            return
        lat, lon = self._screen_to_lonlat(e.pos())
        menu = QMenu(self)
        fly = menu.addAction("✈  Fly To Here")
        fly.setEnabled(o._connected)
        fly.setToolTip("Switch to GUIDED and command the vehicle to this point")
        set_alt = menu.addAction(f"Set guided altitude ({o._guided_alt:.0f} m)…")
        menu.addSeparator()
        clear = menu.addAction("Clear target")
        clear.setEnabled(o._target is not None)
        chosen = menu.exec(e.globalPos())
        if chosen is fly:
            o.fly_to_here(lat, lon)
        elif chosen is set_alt:
            o.ask_guided_alt()
        elif chosen is clear:
            o.clear_target()

    def _screen_to_lonlat(self, pos):
        o = self._o
        z = o._zoom
        cx_px, cy_px = _lonlat_to_world_px(o._center_lat, o._center_lon, z)
        wx = cx_px - self.width() / 2.0 + pos.x()
        wy = cy_px - self.height() / 2.0 + pos.y()
        return self._world_px_to_lonlat(wx, wy, z)

    def _wp_at(self, pos) -> Optional[int]:
        """Index of the waypoint whose dot is under ``pos`` (screen px), or None."""
        o = self._o
        z = o._zoom
        cx_px, cy_px = _lonlat_to_world_px(o._center_lat, o._center_lon, z)
        ox = cx_px - self.width() / 2.0
        oy = cy_px - self.height() / 2.0
        for i, wp in enumerate(o._mission.waypoints):
            px, py = _lonlat_to_world_px(wp.lat, wp.lon, z)
            if (QPointF(px - ox, py - oy) - pos).manhattanLength() < 14:
                return i
        return None

    # ── pan / zoom / waypoint interaction ─────────────────────────────────────
    def mousePressEvent(self, e) -> None:
        if e.button() != Qt.LeftButton:
            return
        o = self._o
        self._press = e.position()
        self._moved = False
        if o._edit:
            idx = self._wp_at(e.position())
            if idx is not None:                # grab an existing waypoint to drag
                o._drag_wp = idx
                return
        o._drag_last = e.position()            # otherwise pan (or add on release)

    def mouseMoveEvent(self, e) -> None:
        o = self._o
        if self._press is not None and \
                (e.position() - self._press).manhattanLength() > 4:
            self._moved = True
        if o._drag_wp is not None:             # dragging a waypoint
            lat, lon = self._screen_to_lonlat(e.position())
            o._mission.move(o._drag_wp, lat, lon)
            o._push_mission()
            return
        if o._drag_last is None:
            return
        delta = e.position() - o._drag_last
        o._drag_last = e.position()
        # panning implies manual control
        if o._follow:
            o._follow = False
            o._follow_btn.setChecked(False)
        z = o._zoom
        cx_px, cy_px = _lonlat_to_world_px(o._center_lat, o._center_lon, z)
        cx_px -= delta.x()
        cy_px -= delta.y()
        o._center_lat, o._center_lon = self._world_px_to_lonlat(cx_px, cy_px, z)
        self.update()

    def mouseReleaseEvent(self, e) -> None:
        o = self._o
        if o._drag_wp is not None:
            idx = o._drag_wp
            o._drag_wp = None
            if not self._moved:                # a tap (no drag) on a waypoint → popup
                o._edit_waypoint(idx)
        elif (e.button() == Qt.LeftButton and o._edit and not self._moved
              and self._press is not None):    # a plain click on empty map → add
            lat, lon = self._screen_to_lonlat(self._press)
            o._mission.add(lat, lon)
            o._push_mission()
        o._drag_last = None
        self._press = None

    def wheelEvent(self, e) -> None:
        o = self._o
        # in edit mode, scrolling over a waypoint tunes its altitude instead of
        # zooming — fine step with Shift, otherwise 5 m per notch
        if o._edit:
            idx = self._wp_at(e.position())
            if idx is not None:
                fine = bool(e.modifiers() & Qt.ShiftModifier)
                d = (1.0 if fine else 5.0) * (1 if e.angleDelta().y() > 0 else -1)
                wp = o._mission.waypoints[idx]
                o._mission.set_alt(idx, max(0.0, wp.alt + d))
                o._push_mission()
                return
        step = 1 if e.angleDelta().y() > 0 else -1
        o._set_zoom(o._zoom + step)

    @staticmethod
    def _world_px_to_lonlat(px: float, py: float, z: int):
        n = 2 ** z
        lon = px / TILE / n * 360.0 - 180.0
        ty = py / TILE / n
        lat = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * ty))))
        return lat, lon
