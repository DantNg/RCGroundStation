"""Small reusable view helpers shared by the panels."""
from __future__ import annotations

from PySide6.QtCore import Qt
from PySide6.QtWidgets import (QFrame, QLabel, QSizePolicy, QVBoxLayout, QWidget)

from . import theme


class Panel(QFrame):
    """A titled, bordered container (the app's basic card)."""

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
