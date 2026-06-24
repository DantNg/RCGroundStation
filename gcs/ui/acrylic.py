"""Frosted-glass ("acrylic") overlay frames — DJI-style floating bars.

The fly-view floats its bars (the top pill, the control panel, the message log)
over a live map/camera. To keep them readable *without* hiding the view, each
bar paints a **blurred** sample of whatever sits behind it, plus a dark tint,
rounded corners and a hairline border — the frosted-glass look DJI's flight app
uses for its top/side strips.

Qt has no CSS ``backdrop-filter``; instead every :class:`AcrylicFrame` grabs the
region of a *backdrop widget* directly beneath it, blurs it cheaply (downscale →
smooth upscale) and draws it as its own background. A module-level backdrop
provider tells the frames which widget to sample (the primary map/camera view),
and each frame nudges itself to repaint on a low-rate timer so the blur tracks
the moving view. The rounded corners stay translucent, so the live view shows
through them and the bars read as glass cards rather than solid boxes.
"""
from __future__ import annotations

from typing import Callable, Optional

from PySide6.QtCore import QPoint, QRect, QRectF, Qt, QTimer
from PySide6.QtGui import (QBrush, QColor, QLinearGradient, QPainter,
                           QPainterPath, QPen, QPixmap)
from PySide6.QtWidgets import QFrame, QGraphicsDropShadowEffect, QWidget

# How aggressively to blur: the grabbed backdrop is shrunk to this fraction and
# smoothly scaled back up. Smaller = softer/cheaper.
_BLUR_DOWNSCALE = 0.18
# Repaint cadence for the live blur (ms). ~11 Hz keeps the glass tracking the
# panning map without burning CPU on a handful of small bars.
_REFRESH_MS = 90

# Tint drawn over the blurred backdrop so text stays legible (frosted glass).
_TINT = QColor(8, 12, 18, 140)
# Fallback fill when no backdrop is available (e.g. the camera's native surface
# can't be grabbed) — a near-solid panel so the bar never turns unreadable.
_TINT_SOLID = QColor(18, 24, 32, 232)
_BORDER = QColor(255, 255, 255, 28)
_RADIUS = 12

_backdrop_provider: Optional[Callable[[], Optional[QWidget]]] = None


def set_backdrop_provider(provider: Callable[[], Optional[QWidget]]) -> None:
    """Register the callable returning the widget the frames should blur.

    Typically ``lambda: <the primary map/camera view>`` — it is queried on every
    repaint so swapping the primary view is picked up automatically.
    """
    global _backdrop_provider
    _backdrop_provider = provider


def _backdrop() -> Optional[QWidget]:
    return _backdrop_provider() if _backdrop_provider is not None else None


def blur_pixmap(pm: QPixmap, downscale: float = _BLUR_DOWNSCALE) -> QPixmap:
    """Cheap Gaussian-ish blur: shrink then smoothly grow back."""
    if pm.isNull():
        return pm
    img = pm.toImage()
    w = max(1, int(img.width() * downscale))
    h = max(1, int(img.height() * downscale))
    small = img.scaled(w, h, Qt.IgnoreAspectRatio, Qt.SmoothTransformation)
    big = small.scaled(img.width(), img.height(),
                       Qt.IgnoreAspectRatio, Qt.SmoothTransformation)
    return QPixmap.fromImage(big)


class AcrylicFrame(QFrame):
    """A frosted-glass card that blurs whatever view sits behind it."""

    def __init__(self, parent=None, radius: int = _RADIUS, shadow: bool = True):
        super().__init__(parent)
        self._radius = radius
        # translucent so the rounded corners reveal the live view beneath us
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        # soft elevation — the card floats above the view (DJI-style)
        if shadow:
            glow = QGraphicsDropShadowEffect(self)
            glow.setBlurRadius(26)
            glow.setColor(QColor(0, 0, 0, 170))
            glow.setOffset(0, 4)
            self.setGraphicsEffect(glow)
        self._refresh = QTimer(self)
        self._refresh.setInterval(_REFRESH_MS)
        self._refresh.timeout.connect(self._maybe_refresh)

    # ── live-blur refresh (only while shown) ─────────────────────────────────
    def showEvent(self, event) -> None:
        super().showEvent(event)
        if _backdrop_provider is not None:
            self._refresh.start()

    def hideEvent(self, event) -> None:
        self._refresh.stop()
        super().hideEvent(event)

    def _maybe_refresh(self) -> None:
        if self.isVisible() and _backdrop() is not None:
            self.update()

    # ── painting ─────────────────────────────────────────────────────────────
    def _backdrop_region(self, src: QWidget) -> QRect:
        top_left = src.mapFromGlobal(self.mapToGlobal(QPoint(0, 0)))
        return QRect(top_left, self.size())

    def paintEvent(self, event) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        rect = QRectF(0.5, 0.5, self.width() - 1, self.height() - 1)
        path = QPainterPath()
        path.addRoundedRect(rect, self._radius, self._radius)
        p.setClipPath(path)

        src = _backdrop()
        drew_backdrop = False
        # never sample a backdrop that contains us — grabbing it would re-enter
        # our own paintEvent. In the fly-view the bars and the primary view are
        # siblings, so this only guards against misuse.
        if (src is not None and src is not self and src.isVisible()
                and not src.isAncestorOf(self)):
            pm = src.grab(self._backdrop_region(src))
            if not pm.isNull():
                p.drawPixmap(self.rect(), blur_pixmap(pm))
                drew_backdrop = True

        # darken/unify so child text stays readable over busy imagery
        p.fillPath(path, _TINT if drew_backdrop else _TINT_SOLID)

        # subtle top sheen — the soft highlight that sells the glass edge
        sheen = QLinearGradient(0, 0, 0, self.height())
        sheen.setColorAt(0.0, QColor(255, 255, 255, 22))
        sheen.setColorAt(0.45, QColor(255, 255, 255, 0))
        p.fillPath(path, QBrush(sheen))

        # hairline border
        p.setClipping(False)
        p.setPen(QPen(_BORDER, 1))
        p.setBrush(Qt.NoBrush)
        p.drawRoundedRect(rect, self._radius, self._radius)
        p.end()
