"""Slippy-map view with the drone marker (Web-Mercator XYZ tiles).

Tiles are fetched on a background thread, cached to disk and in memory, and
drawn under a heading-rotated drone marker plus a breadcrumb trail. The tile
source is pluggable (``TileProvider``) — satellite (Esri) by default, with an
OSM street option — so adding another source never touches the widget.

The firmware fetches one PSRAM image from a PC tool; on the desktop we can
stream standard XYZ tiles directly, so the map pans/zooms freely.
"""
from __future__ import annotations

import math
import queue
import threading
import urllib.request
from dataclasses import dataclass
from pathlib import Path
from typing import Dict, Optional, Set, Tuple

from PySide6.QtCore import QObject, QPointF, Qt, Signal
from PySide6.QtGui import (QBrush, QColor, QFont, QImage, QPainter, QPen,
                           QPolygonF)
from PySide6.QtWidgets import (QComboBox, QHBoxLayout, QInputDialog, QLabel,
                               QMenu, QPushButton, QVBoxLayout, QWidget)

from ..domain.telemetry import TelemetrySnapshot
from . import theme

TILE = 256
_CACHE_DIR = Path.home() / ".lite_gcs" / "tiles"
_USER_AGENT = "LiteGCS-Desktop/1.0 (+https://github.com/)"


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

    def __init__(self, parent=None):
        super().__init__(parent)
        self._provider = PROVIDERS["Satellite"]
        self._zoom = 16
        self._lat = 21.0285
        self._lon = 105.8048
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

        self._loader = _TileLoader()
        self._loader.ready.connect(self.update)

        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)
        outer.addLayout(self._build_controls())
        self._canvas = _MapCanvas(self)
        outer.addWidget(self._canvas, 1)

    def _build_controls(self) -> QHBoxLayout:
        row = QHBoxLayout()
        row.setContentsMargins(6, 4, 6, 4)
        row.addWidget(QLabel("Map:"))
        combo = QComboBox()
        combo.addItems(list(PROVIDERS.keys()))
        combo.currentTextChanged.connect(self._set_provider)
        row.addWidget(combo)
        row.addStretch(1)
        self._follow_btn = QPushButton("Follow: ON")
        self._follow_btn.setCheckable(True)
        self._follow_btn.setChecked(True)
        self._follow_btn.clicked.connect(self._toggle_follow)
        row.addWidget(self._follow_btn)
        minus = QPushButton("−")
        minus.setFixedWidth(34)
        minus.clicked.connect(lambda: self._set_zoom(self._zoom - 1))
        plus = QPushButton("+")
        plus.setFixedWidth(34)
        plus.clicked.connect(lambda: self._set_zoom(self._zoom + 1))
        row.addWidget(minus)
        row.addWidget(plus)
        return row

    # ── data in ────────────────────────────────────────────────────────────
    def update_from(self, s: TelemetrySnapshot) -> None:
        if s.position.valid:
            self._lat = s.position.lat
            self._lon = s.position.lon
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
        self._follow_btn.setText(f"Follow: {'ON' if self._follow else 'OFF'}")
        if self._follow and self._have_fix:
            self._center_lat, self._center_lon = self._lat, self._lon
        self._canvas.update()

    # ── guided interaction (right-click on the map) ──────────────────────────
    def set_connected(self, connected: bool) -> None:
        self._connected = connected

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

    def clear_target(self) -> None:
        self._target = None
        self._canvas.update()


class _MapCanvas(QWidget):
    """The actual tile/marker painter (kept separate from the control row)."""

    def __init__(self, owner: MapWidget):
        super().__init__(owner)
        self._o = owner
        self.setMinimumSize(320, 240)
        self.setMouseTracking(True)

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
        self._draw_target(p, origin_x, origin_y, z)
        self._draw_marker(p, origin_x, origin_y, z)
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

    def _draw_hud_text(self, p) -> None:
        o = self._o
        p.setFont(QFont("Consolas", 9))
        if o._have_fix:
            txt = f"{o._lat:.6f}, {o._lon:.6f}   z{o._zoom}"
        else:
            txt = f"no GPS fix   z{o._zoom}"
        # bottom-right, clear of the floating round HUD (bottom-left)
        tw = len(txt) * 7 + 10
        x = self.width() - tw - 6
        p.fillRect(x, self.height() - 24, tw, 18, QColor(0, 0, 0, 140))
        p.setPen(QPen(QColor(theme.TEXT)))
        p.drawText(x + 4, self.height() - 10, txt)

    # ── guided right-click menu ──────────────────────────────────────────────
    def contextMenuEvent(self, e) -> None:
        o = self._o
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

    # ── pan / zoom interaction ────────────────────────────────────────────────
    def mousePressEvent(self, e) -> None:
        if e.button() == Qt.LeftButton:
            self._o._drag_last = e.position()

    def mouseMoveEvent(self, e) -> None:
        o = self._o
        if o._drag_last is None:
            return
        delta = e.position() - o._drag_last
        o._drag_last = e.position()
        # panning implies manual control
        if o._follow:
            o._follow = False
            o._follow_btn.setChecked(False)
            o._follow_btn.setText("Follow: OFF")
        z = o._zoom
        cx_px, cy_px = _lonlat_to_world_px(o._center_lat, o._center_lon, z)
        cx_px -= delta.x()
        cy_px -= delta.y()
        o._center_lat, o._center_lon = self._world_px_to_lonlat(cx_px, cy_px, z)
        self.update()

    def mouseReleaseEvent(self, _e) -> None:
        self._o._drag_last = None

    def wheelEvent(self, e) -> None:
        step = 1 if e.angleDelta().y() > 0 else -1
        self._o._set_zoom(self._o._zoom + step)

    @staticmethod
    def _world_px_to_lonlat(px: float, py: float, z: int):
        n = 2 ** z
        lon = px / TILE / n * 360.0 - 180.0
        ty = py / TILE / n
        lat = math.degrees(math.atan(math.sinh(math.pi * (1 - 2 * ty))))
        return lat, lon
