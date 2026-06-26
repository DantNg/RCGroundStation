"""Touch-friendly popup for editing one waypoint's altitude (or deleting it).

Tapping a waypoint on the 2D map or the 3D globe opens this small modal dialog
instead of relying on a scroll wheel or a right-click menu — the big ``−``/``+``
steppers and a numeric field work with a finger on a touchscreen. It edits live:
every change emits :attr:`alt_changed` so the map updates underneath, and
:attr:`delete_requested` removes the point.
"""
from __future__ import annotations

from PySide6.QtCore import Qt, Signal
from PySide6.QtWidgets import (QDoubleSpinBox, QHBoxLayout, QLabel, QPushButton,
                               QVBoxLayout, QDialog)


class WaypointEditor(QDialog):
    alt_changed = Signal(float)     # new altitude (m)
    delete_requested = Signal()

    def __init__(self, idx: int, alt: float, parent=None):
        super().__init__(parent)
        self.setWindowTitle(f"Waypoint {idx + 1}")
        self.setModal(True)

        col = QVBoxLayout(self)
        col.setContentsMargins(16, 14, 16, 14)
        col.setSpacing(12)

        title = QLabel(f"Waypoint {idx + 1} — altitude")
        title.setObjectName("PanelTitle")
        col.addWidget(title)

        # ── big −  [value m]  + stepper (5 m per tap) ────────────────────────
        row = QHBoxLayout()
        row.setSpacing(10)
        minus = QPushButton("−")
        minus.setObjectName("IconButton")
        minus.setMinimumSize(56, 56)
        plus = QPushButton("+")
        plus.setObjectName("IconButton")
        plus.setMinimumSize(56, 56)
        self._spin = QDoubleSpinBox()
        self._spin.setRange(0.0, 1000.0)
        self._spin.setDecimals(0)
        self._spin.setSingleStep(1.0)
        self._spin.setSuffix(" m")
        self._spin.setValue(alt)
        self._spin.setAlignment(Qt.AlignCenter)
        self._spin.setMinimumHeight(56)
        f = self._spin.font()
        f.setPointSize(f.pointSize() + 6)
        f.setBold(True)
        self._spin.setFont(f)
        minus.clicked.connect(lambda: self._spin.setValue(self._spin.value() - 5))
        plus.clicked.connect(lambda: self._spin.setValue(self._spin.value() + 5))
        self._spin.valueChanged.connect(self.alt_changed.emit)
        row.addWidget(minus)
        row.addWidget(self._spin, 1)
        row.addWidget(plus)
        col.addLayout(row)

        # ── delete / done ────────────────────────────────────────────────────
        actions = QHBoxLayout()
        actions.setSpacing(10)
        delete = QPushButton("🗑  Delete")
        delete.setObjectName("Disarm")
        delete.setMinimumHeight(44)
        delete.clicked.connect(self._on_delete)
        done = QPushButton("Done")
        done.setMinimumHeight(44)
        done.clicked.connect(self.accept)
        actions.addWidget(delete)
        actions.addWidget(done, 1)
        col.addLayout(actions)

    def _on_delete(self) -> None:
        self.delete_requested.emit()
        self.reject()
