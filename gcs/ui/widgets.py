"""Small reusable view helpers shared by the panels."""
from __future__ import annotations

from PySide6.QtCore import QRectF, Qt, Signal
from PySide6.QtGui import QColor, QFont, QPainter, QPen
from PySide6.QtWidgets import QLabel, QSizePolicy, QVBoxLayout, QWidget

from . import theme
from .acrylic import AcrylicFrame


class Panel(AcrylicFrame):
    """A titled container — a frosted-glass card floating over the fly-view."""

    def __init__(self, title: str, parent=None):
        super().__init__(parent)
        self.setObjectName("Panel")
        self._outer = QVBoxLayout(self)
        self._outer.setContentsMargins(10, 8, 10, 10)
        self._outer.setSpacing(6)
        if title:
            lbl = QLabel(title)
            lbl.setObjectName("PanelTitle")
            self._outer.addWidget(lbl)

    def body(self) -> QVBoxLayout:
        return self._outer


class Stat(QWidget):
    """A label + value row, e.g. ``Battery   12.4 V``."""

    def __init__(self, name: str, parent=None):
        super().__init__(parent)
        from PySide6.QtWidgets import QHBoxLayout
        lay = QHBoxLayout(self)
        lay.setContentsMargins(0, 0, 0, 0)
        self._name = QLabel(name)
        self._name.setStyleSheet(f"color: {theme.TEXT_DIM};")
        self._value = QLabel("—")
        self._value.setAlignment(Qt.AlignRight | Qt.AlignVCenter)
        self._value.setStyleSheet("font-weight: 600;")
        self._value.setSizePolicy(QSizePolicy.Expanding, QSizePolicy.Preferred)
        lay.addWidget(self._name)
        lay.addWidget(self._value, 1)

    def set(self, text: str, color: str = theme.TEXT) -> None:
        self._value.setText(text)
        self._value.setStyleSheet(f"font-weight: 600; color: {color};")


class PipOverlay(QWidget):
    """Transparent clickable frame placed over the picture-in-picture tile.

    It draws a rounded outline (brighter on hover), a corner *expand* badge and a
    caption naming the small view, and emits :attr:`clicked` when pressed — that
    is the "tap the small view to make it the main view" affordance. It sits on
    top of the PIP widget so the click never falls through to e.g. map panning.
    """

    clicked = Signal()

    def __init__(self, parent=None):
        super().__init__(parent)
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self.setCursor(Qt.PointingHandCursor)
        self._hover = False
        self._caption = ""

    def set_caption(self, text: str) -> None:
        if text != self._caption:
            self._caption = text
            self.update()

    def enterEvent(self, _e) -> None:
        self._hover = True
        self.update()

    def leaveEvent(self, _e) -> None:
        self._hover = False
        self.update()

    def mouseReleaseEvent(self, e) -> None:
        if e.button() == Qt.LeftButton and self.rect().contains(e.position().toPoint()):
            self.clicked.emit()

    def paintEvent(self, _e) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        w, h = self.width(), self.height()
        accent = QColor(theme.ACCENT2)
        border = QColor(accent if self._hover else QColor(255, 255, 255, 70))
        p.setPen(QPen(border, 2 if self._hover else 1.5))
        p.setBrush(Qt.NoBrush)
        p.drawRoundedRect(QRectF(1, 1, w - 2, h - 2), 10, 10)

        # expand badge (top-right)
        bs = 22
        bx, by = w - bs - 7, 7
        p.setPen(Qt.NoPen)
        p.setBrush(QColor(10, 14, 20, 200))
        p.drawRoundedRect(QRectF(bx, by, bs, bs), 6, 6)
        p.setPen(QPen(QColor("#ffffff"), 2))
        p.setFont(QFont("Segoe UI", 11, QFont.Bold))
        p.drawText(QRectF(bx, by, bs, bs), Qt.AlignCenter, "⤢")

        # caption (bottom-left)
        if self._caption:
            p.setFont(QFont("Segoe UI", 9, QFont.Bold))
            tw = max(46, 14 + len(self._caption) * 8)
            p.setPen(Qt.NoPen)
            p.setBrush(QColor(10, 14, 20, 200))
            p.drawRoundedRect(QRectF(7, h - 26, tw, 19), 6, 6)
            p.setPen(QPen(QColor("#e6edf3")))
            p.drawText(QRectF(7, h - 26, tw, 19), Qt.AlignCenter, self._caption)
        p.end()
