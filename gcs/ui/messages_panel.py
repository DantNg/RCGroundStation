"""Mission-Planner-style message log.

Every STATUSTEXT from the vehicle, every COMMAND_ACK result, and every local
notice (connect/disconnect, command sent, errors) lands here, colour-coded by
severity and timestamped. Errors/warnings also bubble up to a one-line banner
the MainWindow overlays near the HUD, just like Mission Planner's red HUD text.
"""
from __future__ import annotations

import time
from html import escape

from PySide6.QtWidgets import QHBoxLayout, QPushButton, QTextEdit

from ..domain.telemetry import Severity, StatusText
from . import theme
from .widgets import Panel

_MAX_BLOCKS = 500


class MessagesPanel(Panel):
    def __init__(self, parent=None):
        super().__init__("MESSAGES", parent)
        body = self.body()

        self._log = QTextEdit()
        self._log.setReadOnly(True)
        self._log.document().setMaximumBlockCount(_MAX_BLOCKS)
        body.addWidget(self._log, 1)

        row = QHBoxLayout()
        row.addStretch(1)
        clear = QPushButton("Clear")
        clear.clicked.connect(self._log.clear)
        row.addWidget(clear)
        body.addLayout(row)

    def add(self, st: StatusText) -> None:
        ts = time.strftime("%H:%M:%S")
        color = theme.severity_color(st.severity)
        sev = st.severity.label.upper()[:4]
        line = (f'<span style="color:{theme.TEXT_DIM}">[{ts}]</span> '
                f'<span style="color:{color}; font-weight:600">{sev:<5}</span> '
                f'<span style="color:{color}">{escape(st.text)}</span>')
        self._log.append(line)
        bar = self._log.verticalScrollBar()
        bar.setValue(bar.maximum())

    @staticmethod
    def is_alert(st: StatusText) -> bool:
        return st.severity <= Severity.WARNING
