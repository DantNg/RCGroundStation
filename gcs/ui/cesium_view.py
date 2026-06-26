"""3D map view — a Cesium globe hosted in a QWebEngineView.

This wraps the bundled ``assets/cesium_map.html`` page and bridges it to the rest
of the (pure-Qt) app. Python pushes vehicle position, the waypoint list and the
simulation marker into the page by calling its ``window.gcs*`` functions; the
page pushes user edits (add / move / remove a waypoint) back through a
:class:`QWebChannel` object named ``bridge``. Calls made before the page has
finished loading are queued and flushed on ``loadFinished`` so the caller never
has to care about readiness.

Cesium itself is **vendored** under ``assets/cesium/`` and served by a small
localhost HTTP server (started once, shared by all views). Cesium loads its
maths data and decoders from Web Workers, which browsers refuse to start from a
``file://`` origin, so a real http origin is required — and serving it locally
means the 3D engine works **fully offline** (only the satellite imagery needs the
network, exactly like the 2D map).

The 2D :class:`~gcs.ui.map_widget.MapWidget` owns the mission; this view is a
second renderer of the same data, so the two stay in lock-step.
"""
from __future__ import annotations

import functools
import json
import threading
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from typing import List, Optional

from PySide6.QtCore import QObject, QUrl, Signal, Slot
from PySide6.QtWebChannel import QWebChannel
from PySide6.QtWebEngineCore import QWebEnginePage
from PySide6.QtWebEngineWidgets import QWebEngineView

_ASSETS = Path(__file__).parent / "assets"

# A process-wide static file server for the vendored Cesium build + the page.
_server_lock = threading.Lock()
_server_base: Optional[str] = None   # e.g. "http://127.0.0.1:53124"


class _QuietHandler(SimpleHTTPRequestHandler):
    """Static handler rooted at the assets dir, with WASM mime + no logging."""

    extensions_map = {**SimpleHTTPRequestHandler.extensions_map,
                      ".wasm": "application/wasm", ".json": "application/json"}

    def log_message(self, *_args) -> None:  # silence per-request stderr spam
        pass


def _ensure_server() -> str:
    """Start (once) the localhost server for assets/ and return its base URL."""
    global _server_base
    with _server_lock:
        if _server_base is None:
            handler = functools.partial(_QuietHandler, directory=str(_ASSETS))
            httpd = ThreadingHTTPServer(("127.0.0.1", 0), handler)
            threading.Thread(target=httpd.serve_forever,
                             name="cesium-http", daemon=True).start()
            host, port = httpd.server_address
            _server_base = f"http://{host}:{port}"
        return _server_base


class _LoggingPage(QWebEnginePage):
    """Forwards the page's JS ``console`` output to the terminal for debugging.

    The Cesium page logs ``[gcs] …`` status/error lines; surfacing them here turns
    an otherwise-silent black globe into a readable diagnostic (WebGL missing,
    Cesium CDN unreachable, etc.) without a browser dev-tools window.
    """

    def javaScriptConsoleMessage(self, level, message, line, source):  # noqa: N802
        print(f"[cesium] {message}")


class _Bridge(QObject):
    """JS → Python edit events (registered on the page as ``bridge``)."""

    waypoint_added = Signal(float, float)          # lat, lon
    waypoint_moved = Signal(int, float, float)     # idx, lat, lon
    waypoint_removed = Signal(int)                 # idx
    waypoint_alt_changed = Signal(int, float)      # idx, delta_m
    waypoint_clicked = Signal(int)                 # idx — open the edit popup

    @Slot(float, float)
    def add_waypoint(self, lat: float, lon: float) -> None:
        self.waypoint_added.emit(lat, lon)

    @Slot(int)
    def select_waypoint(self, idx: int) -> None:
        self.waypoint_clicked.emit(idx)

    @Slot(int, float, float)
    def move_waypoint(self, idx: int, lat: float, lon: float) -> None:
        self.waypoint_moved.emit(idx, lat, lon)

    @Slot(int)
    def remove_waypoint(self, idx: int) -> None:
        self.waypoint_removed.emit(idx)

    @Slot(int, float)
    def adjust_waypoint_alt(self, idx: int, delta: float) -> None:
        self.waypoint_alt_changed.emit(idx, delta)


class CesiumView(QWebEngineView):
    # re-exposed so MapWidget can wire them like any Qt signal
    waypoint_added = Signal(float, float)
    waypoint_moved = Signal(int, float, float)
    waypoint_removed = Signal(int)
    waypoint_alt_changed = Signal(int, float)
    waypoint_clicked = Signal(int)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._ready = False
        self._pending: List[str] = []

        self._page = _LoggingPage(self)
        self.setPage(self._page)

        self._bridge = _Bridge()
        self._bridge.waypoint_added.connect(self.waypoint_added)
        self._bridge.waypoint_moved.connect(self.waypoint_moved)
        self._bridge.waypoint_removed.connect(self.waypoint_removed)
        self._bridge.waypoint_alt_changed.connect(self.waypoint_alt_changed)
        self._bridge.waypoint_clicked.connect(self.waypoint_clicked)

        self._channel = QWebChannel(self)
        self._channel.registerObject("bridge", self._bridge)
        self.page().setWebChannel(self._channel)

        self.loadFinished.connect(self._on_loaded)
        self.load(QUrl(f"{_ensure_server()}/cesium_map.html"))

    # ── readiness / JS plumbing ───────────────────────────────────────────────
    def _on_loaded(self, ok: bool) -> None:
        self._ready = bool(ok)
        if not ok:
            return
        for js in self._pending:
            self.page().runJavaScript(js)
        self._pending.clear()

    def _js(self, code: str) -> None:
        if self._ready:
            self.page().runJavaScript(code)
        else:
            self._pending.append(code)

    # ── Python → JS API ───────────────────────────────────────────────────────
    def set_vehicle(self, lat: float, lon: float, alt: float,
                    heading: float, have_fix: bool) -> None:
        self._js(f"gcsSetVehicle({lat},{lon},{alt},{heading},{str(bool(have_fix)).lower()})")

    def set_waypoints(self, waypoints: List[dict]) -> None:
        self._js(f"gcsSetWaypoints({json.dumps(waypoints)})")

    def set_sim(self, lat: float, lon: float, alt: float, heading: float) -> None:
        self._js(f"gcsSetSim({lat},{lon},{alt},{heading})")

    def clear_sim(self) -> None:
        self._js("gcsClearSim()")

    def set_edit_mode(self, on: bool) -> None:
        self._js(f"gcsSetEdit({str(bool(on)).lower()})")

    def set_follow(self, on: bool) -> None:
        self._js(f"gcsSetFollow({str(bool(on)).lower()})")

    def fly_to_vehicle(self) -> None:
        self._js("gcsFlyToVehicle()")
