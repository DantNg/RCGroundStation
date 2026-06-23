"""Live webcam view — a second full-screen-capable view alongside the map.

Uses Qt Multimedia (``QCamera`` → ``QMediaCaptureSession`` → ``QVideoWidget``)
so it works on Windows / Linux with whatever cameras the OS exposes. The view is
self-contained: a header strip picks the device and starts/stops the feed, and a
placeholder is shown until a frame is flowing. In picture-in-picture mode the
header is hidden so only the live image (or placeholder) fills the small tile.

It is deliberately passive about layout — the MainWindow decides whether it is
the primary view or the corner PIP and calls :meth:`set_pip` accordingly.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QComboBox, QHBoxLayout, QLabel, QPushButton,
                               QStackedLayout, QVBoxLayout, QWidget)

try:
    from PySide6.QtMultimedia import (QCamera, QMediaCaptureSession,
                                      QMediaDevices)
    from PySide6.QtMultimediaWidgets import QVideoWidget
    _MULTIMEDIA_OK = True
except Exception:  # pragma: no cover - depends on the PySide6 build
    _MULTIMEDIA_OK = False


class CameraView(QWidget):
    """A webcam panel that can act as the main view or shrink to a PIP tile."""

    running_changed = Signal(bool)

    def __init__(self, parent=None):
        super().__init__(parent)
        self._running = False
        self._camera = None
        self._media_devices = None
        self._session = None
        self._video = None

        outer = QVBoxLayout(self)
        outer.setContentsMargins(0, 0, 0, 0)
        outer.setSpacing(0)

        # The live video fills the whole widget; its device/start controls live
        # in a separate header that the MainWindow hosts in the shared top pill.
        self._header = self._build_header()

        # body: live video on top of a placeholder (placeholder shows when off)
        body = QWidget()
        self._stack = QStackedLayout(body)
        self._stack.setStackingMode(QStackedLayout.StackAll)
        self._stack.setContentsMargins(0, 0, 0, 0)

        self._placeholder = QLabel("Camera off")
        self._placeholder.setObjectName("CamPlaceholder")
        self._placeholder.setAlignment(Qt.AlignCenter)
        self._placeholder.setWordWrap(True)
        self._placeholder.setStyleSheet("background-color: #05080c;")
        self._stack.addWidget(self._placeholder)

        if _MULTIMEDIA_OK:
            self._session = QMediaCaptureSession()
            self._video = QVideoWidget()
            self._video.setStyleSheet("background-color: #05080c;")
            self._session.setVideoOutput(self._video)
            self._stack.addWidget(self._video)
            self._media_devices = QMediaDevices(self)
            self._media_devices.videoInputsChanged.connect(self._refresh_devices)

        outer.addWidget(body, 1)

        if _MULTIMEDIA_OK:
            self._refresh_devices()
        else:
            self._device.setEnabled(False)
            self._start_btn.setEnabled(False)
            self._placeholder.setText("Qt Multimedia unavailable")
        self._show_video(False)

    def header_widget(self) -> QWidget:
        """The camera's device/start controls (hosted in the shared top pill)."""
        return self._header

    def set_header_compact(self, compact: bool) -> None:
        self._cam_label.setVisible(not compact)

    # ── keep the placeholder text legible at any tile size ───────────────────
    def resizeEvent(self, e) -> None:
        super().resizeEvent(e)
        side = min(self.width(), self.height())
        px = max(8, min(20, int(side * 0.12)))
        # set via inline QSS — a per-widget stylesheet overrides the app-level
        # `QWidget { font-size }`, which a plain setFont() would not.
        self._placeholder.setStyleSheet(f"background-color: #05080c; font-size: {px}px;")

    # ── header ────────────────────────────────────────────────────────────────
    def _build_header(self) -> QWidget:
        bar = QWidget()
        bar.setAttribute(Qt.WA_TranslucentBackground, True)
        row = QHBoxLayout(bar)
        row.setContentsMargins(0, 0, 0, 0)
        row.setSpacing(6)

        self._cam_label = QLabel("CAM")
        self._cam_label.setObjectName("PanelTitle")
        row.addWidget(self._cam_label)

        self._device = QComboBox()
        self._device.setMinimumWidth(96)
        self._device.currentIndexChanged.connect(self._on_device_changed)
        row.addWidget(self._device, 1)

        refresh = QPushButton("⟳")
        refresh.setObjectName("IconButton")
        refresh.setToolTip("Refresh camera list")
        refresh.setCursor(Qt.PointingHandCursor)
        refresh.clicked.connect(self._refresh_devices)
        row.addWidget(refresh)

        self._start_btn = QPushButton("Start")
        self._start_btn.setObjectName("Ghost")
        self._start_btn.setCursor(Qt.PointingHandCursor)
        self._start_btn.clicked.connect(self._toggle)
        row.addWidget(self._start_btn)
        return bar

    # ── device handling ─────────────────────────────────────────────────────
    def _refresh_devices(self) -> None:
        if not _MULTIMEDIA_OK:
            return
        prev = self._device.currentData()
        self._device.blockSignals(True)
        self._device.clear()
        cams = QMediaDevices.videoInputs()
        for cam in cams:
            self._device.addItem(cam.description(), cam)
        self._device.blockSignals(False)
        if not cams:
            self._placeholder.setText("No camera")
            self._start_btn.setEnabled(False)
            return
        self._start_btn.setEnabled(True)
        # keep the previous selection if it's still around
        for i in range(self._device.count()):
            dev = self._device.itemData(i)
            if prev is not None and dev.id() == prev.id():
                self._device.setCurrentIndex(i)
                break
        if not self._running:
            self._placeholder.setText("Camera off")

    def _on_device_changed(self, _idx: int) -> None:
        if self._running:
            # restart on the newly selected device
            self._stop()
            self.ensure_started()

    # ── start / stop ─────────────────────────────────────────────────────────
    def _toggle(self) -> None:
        if self._running:
            self._stop()
        else:
            self.ensure_started()

    def ensure_started(self) -> bool:
        """Start the selected camera if possible. Safe to call repeatedly."""
        if not _MULTIMEDIA_OK or self._running:
            return self._running
        dev = self._device.currentData()
        if dev is None:
            return False
        try:
            self._camera = QCamera(dev)
            self._camera.errorOccurred.connect(self._on_camera_error)
            self._session.setCamera(self._camera)
            self._camera.start()
        except Exception as exc:  # pragma: no cover - hardware/driver dependent
            self._placeholder.setText(f"Camera error:\n{exc}")
            self._camera = None
            return False
        self._set_running(True)
        return True

    def _stop(self) -> None:
        if self._camera is not None:
            try:
                self._camera.stop()
            except Exception:
                pass
            self._session.setCamera(None)
            self._camera = None
        self._set_running(False)
        self._placeholder.setText("Camera off")

    def _on_camera_error(self, _err, msg: str) -> None:  # pragma: no cover
        self._placeholder.setText(f"Camera error:\n{msg or 'unavailable'}")
        self._set_running(False)

    def _set_running(self, running: bool) -> None:
        if running == self._running:
            self._show_video(running)
            return
        self._running = running
        self._start_btn.setText("Stop" if running else "Start")
        self._show_video(running)
        self.running_changed.emit(running)

    def _show_video(self, show: bool) -> None:
        if self._video is not None:
            self._video.setVisible(show)
        self._placeholder.setVisible(not show)

    def is_running(self) -> bool:
        return self._running

    # ── lifecycle ─────────────────────────────────────────────────────────────
    def shutdown(self) -> None:
        self._stop()
