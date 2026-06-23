"""A container that floats children over a full-bleed base, owned by the caller.

Used for the fly-view: a primary view (map *or* camera) fills the stage while
the round HUD, control panel, message log, the corner picture-in-picture tile
and the connection chip float over it. The stage stays dumb — it only reparents
the widgets it is given and, on every resize, hands its size to a layout
callback so the owner (MainWindow) decides placement, z-order and responsive
hide/show in one place.
"""
from __future__ import annotations

from typing import Callable

from PySide6.QtWidgets import QWidget


class OverlayStage(QWidget):
    def __init__(self, on_layout: Callable[[int, int], None], parent=None):
        super().__init__(parent)
        self._on_layout = on_layout

    def add(self, widget: QWidget) -> QWidget:
        """Reparent a widget onto the stage (added in back-to-front order)."""
        widget.setParent(self)
        return widget

    def resizeEvent(self, event) -> None:
        self._on_layout(self.width(), self.height())
        super().resizeEvent(event)
