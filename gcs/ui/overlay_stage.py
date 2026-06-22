"""A container that fills itself with one base widget and floats overlays on top.

Used for the fly-view: the map is the base (fills the whole stage) and the round
HUD, control panel and message log float over it. Positioning is delegated to a
layout callback so the owner (MainWindow) decides placement and responsive
hide/show in one place.
"""
from __future__ import annotations

from typing import Callable

from PySide6.QtWidgets import QWidget


class OverlayStage(QWidget):
    def __init__(self, base: QWidget, on_layout: Callable[[int, int], None], parent=None):
        super().__init__(parent)
        self._base = base
        self._base.setParent(self)
        self._on_layout = on_layout

    def add_overlay(self, widget: QWidget) -> None:
        widget.setParent(self)
        widget.raise_()

    def resizeEvent(self, event) -> None:
        self._base.setGeometry(self.rect())
        self._on_layout(self.width(), self.height())
        super().resizeEvent(event)
