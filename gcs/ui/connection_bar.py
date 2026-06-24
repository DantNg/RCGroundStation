"""Top connection bar: choose transport (serial / UDP / TCP) and connect.

Emits a fully-formed :class:`~gcs.config.AppConfig` so the MainWindow stays
ignorant of widget plumbing. Serial ports are enumerated with pyserial.
"""
from __future__ import annotations

from PySide6.QtCore import Signal
from PySide6.QtWidgets import (QComboBox, QFrame, QHBoxLayout, QLabel, QLineEdit,
                               QPushButton, QSpinBox, QStackedWidget, QWidget)

from ..config import AppConfig
from . import theme
from .acrylic import AcrylicFrame

_BAUDS = ["9600", "19200", "38400", "57600", "115200", "230400", "921600"]


def _list_serial_ports():
    try:
        from serial.tools import list_ports
        return [p.device for p in list_ports.comports()]
    except Exception:
        return []


class ConnectionBar(AcrylicFrame):
    connect_requested = Signal(object)   # AppConfig
    disconnect_requested = Signal()

    def __init__(self, cfg: AppConfig, parent=None):
        super().__init__(parent)
        self.setObjectName("ConnBar")
        self._connected = False
        lay = QHBoxLayout(self)
        lay.setContentsMargins(14, 8, 14, 8)
        lay.setSpacing(8)

        # leading status indicator (DJI-style "Disconnected" chip)
        self._dot = QLabel("●")
        self._dot.setStyleSheet(f"color: {theme.BAD}; font-size: 13px;")
        self._status = QLabel("Disconnected")
        self._status.setStyleSheet("font-weight: 600;")
        self._vsep = QFrame()
        self._vsep.setFrameShape(QFrame.VLine)
        self._vsep.setStyleSheet(f"color: {theme.PANEL_BORDER};")
        lay.addWidget(self._dot)
        lay.addWidget(self._status)
        lay.addWidget(self._vsep)

        lay.addWidget(QLabel("Link:"))
        self._type = QComboBox()
        self._type.addItems(["Serial", "UDP", "TCP"])
        self._type.currentIndexChanged.connect(self._on_type_changed)
        lay.addWidget(self._type)

        self._stack = QStackedWidget()
        self._stack.addWidget(self._build_serial())
        self._stack.addWidget(self._build_udp())
        self._stack.addWidget(self._build_tcp())
        lay.addWidget(self._stack)
        lay.addStretch(1)

        self._connect_btn = QPushButton("Connect")
        self._connect_btn.setObjectName("Connect")
        self._connect_btn.clicked.connect(self._on_connect_clicked)
        lay.addWidget(self._connect_btn)

        self._apply_config(cfg)

    # ── per-transport input pages ────────────────────────────────────────────
    def _build_serial(self) -> QWidget:
        w = QWidget()
        row = QHBoxLayout(w)
        row.setContentsMargins(0, 0, 0, 0)
        self._port = QComboBox()
        self._port.setEditable(True)
        self._port.setMinimumWidth(140)
        refresh = QPushButton("⟳")
        refresh.setFixedWidth(34)
        refresh.setToolTip("Refresh serial ports")
        refresh.clicked.connect(self._refresh_ports)
        self._baud = QComboBox()
        self._baud.addItems(_BAUDS)
        self._baud.setCurrentText("57600")
        row.addWidget(QLabel("Port:"))
        row.addWidget(self._port)
        row.addWidget(refresh)
        row.addWidget(QLabel("Baud:"))
        row.addWidget(self._baud)
        self._refresh_ports()
        return w

    def _build_udp(self) -> QWidget:
        w = QWidget()
        row = QHBoxLayout(w)
        row.setContentsMargins(0, 0, 0, 0)
        self._udp_port = QSpinBox()
        self._udp_port.setRange(1, 65535)
        self._udp_port.setValue(14550)
        row.addWidget(QLabel("Listen UDP port:"))
        row.addWidget(self._udp_port)
        return w

    def _build_tcp(self) -> QWidget:
        w = QWidget()
        row = QHBoxLayout(w)
        row.setContentsMargins(0, 0, 0, 0)
        self._tcp_host = QLineEdit("127.0.0.1")
        self._tcp_host.setFixedWidth(120)
        self._tcp_port = QSpinBox()
        self._tcp_port.setRange(1, 65535)
        self._tcp_port.setValue(5760)
        row.addWidget(QLabel("Host:"))
        row.addWidget(self._tcp_host)
        row.addWidget(QLabel("Port:"))
        row.addWidget(self._tcp_port)
        return w

    # ── state ─────────────────────────────────────────────────────────────
    def set_connected(self, connected: bool) -> None:
        self._connected = connected
        self._connect_btn.setText("Disconnect" if connected else "Connect")
        self._type.setEnabled(not connected)
        self._stack.setEnabled(not connected)
        color = theme.GOOD if connected else theme.BAD
        self._dot.setStyleSheet(f"color: {color}; font-size: 13px;")
        self._status.setText("Connected" if connected else "Disconnected")

    def current_config(self) -> AppConfig:
        kind = ["serial", "udp", "tcp"][self._type.currentIndex()]
        return AppConfig(
            connection_type=kind,
            serial_port=self._port.currentText().strip(),
            baud=int(self._baud.currentText()),
            udp_port=self._udp_port.value(),
            tcp_host=self._tcp_host.text().strip() or "127.0.0.1",
            tcp_port=self._tcp_port.value(),
        )

    # ── handlers ────────────────────────────────────────────────────────────
    def _refresh_ports(self) -> None:
        current = self._port.currentText()
        self._port.clear()
        ports = _list_serial_ports()
        self._port.addItems(ports)
        if current:
            self._port.setCurrentText(current)

    def _on_type_changed(self, idx: int) -> None:
        self._stack.setCurrentIndex(idx)

    def _on_connect_clicked(self) -> None:
        if self._connected:
            self.disconnect_requested.emit()
        else:
            self.connect_requested.emit(self.current_config())

    def _apply_config(self, cfg: AppConfig) -> None:
        self._type.setCurrentIndex({"serial": 0, "udp": 1, "tcp": 2}.get(cfg.connection_type, 0))
        if cfg.serial_port:
            self._port.setCurrentText(cfg.serial_port)
        self._baud.setCurrentText(str(cfg.baud))
        self._udp_port.setValue(cfg.udp_port)
        self._tcp_host.setText(cfg.tcp_host)
        self._tcp_port.setValue(cfg.tcp_port)
