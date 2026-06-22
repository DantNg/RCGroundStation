"""On-map warning banner stack.

Shows the most recent warnings/errors (severity ≤ WARNING) as colour-coded
chips floating over the map, so the operator never has to look away from the
flight area to catch a pre-arm failure or a battery alarm. Entries fade out
after a few seconds. The widget is click-through, so it never steals the map's
right-click "Fly To Here".
"""
from __future__ import annotations

import time

from PySide6.QtCore import QRectF, Qt
from PySide6.QtGui import QBrush, QColor, QFont, QFontMetrics, QPainter, QPen
from PySide6.QtWidgets import QWidget

from ..domain.telemetry import StatusText
from . import theme

_HOLD_MS = 9000
_MAX_ITEMS = 4


class WarningOverlay(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        # float over the map, never intercept its mouse (Fly-To right-click)
        self.setAttribute(Qt.WA_TransparentForMouseEvents, True)
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self._items: list[list] = []   # [text, QColor, expire_ms]

    def push(self, st: StatusText) -> None:
        expire = int(time.monotonic() * 1000) + _HOLD_MS
        self._items.insert(0, [st.text, QColor(theme.severity_color(st.severity)), expire])
        del self._items[_MAX_ITEMS:]
        self.update()

    def prune(self) -> None:
        now = int(time.monotonic() * 1000)
        keep = [it for it in self._items if it[2] > now]
        if len(keep) != len(self._items):
            self._items = keep
            self.update()

    def has_items(self) -> bool:
        return bool(self._items)

    def paintEvent(self, _e) -> None:
        if not self._items:
            return
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        p.setRenderHint(QPainter.TextAntialiasing, True)
        w = self.width()
        font = QFont("Segoe UI", 11, QFont.Bold)
        p.setFont(font)
        fm = QFontMetrics(font)
        chip_h = fm.height() + 12
        gap = 6
        y = 0
        for text, color, _exp in self._items:
            rect = QRectF(0, y, w, chip_h)
            bg = QColor(color)
            bg.setAlpha(225)
            p.setPen(Qt.NoPen)
            p.setBrush(QBrush(bg))
            p.drawRoundedRect(rect, 6, 6)
            label = "⚠  " + fm.elidedText(text, Qt.ElideRight, w - 40)
            p.setPen(QPen(QColor("#ffffff")))
            p.drawText(rect.adjusted(12, 0, -10, 0), Qt.AlignVCenter | Qt.AlignLeft, label)
            y += chip_h + gap
        p.end()
