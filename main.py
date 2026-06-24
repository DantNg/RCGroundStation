#!/usr/bin/env python3
"""Lite Ground Station — desktop entry point (Windows / Linux).

    python main.py

The window opens disconnected; pick a link (Serial / UDP / TCP) in the top bar
and press Connect. To try it without hardware, run the bundled simulator in a
second terminal and connect via UDP on port 14550:

    python tools/sim_udp.py
"""
from __future__ import annotations

import sys

from PySide6.QtWidgets import QApplication

from gcs.app.controller import GcsController
from gcs.config import AppConfig
from gcs.ui.main_window import MainWindow
from PySide6.QtCore import Qt


def main() -> int:
    app = QApplication(sys.argv)
    app.setApplicationName("Lite Ground Station")
    app.setApplicationDisplayName("Lite Ground Station — Desktop")

    config = AppConfig.load()
    controller = GcsController()
    window = MainWindow(controller, config)
    window.setWindowFlags(
        Qt.Window |
        Qt.FramelessWindowHint
    )

    window.showFullScreen()
    # window.show()
    return app.exec()


if __name__ == "__main__":
    sys.exit(main())
