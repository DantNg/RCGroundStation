"""User-facing connection settings, persisted to a small JSON file.

The desktop analogue of the firmware's ``AppConfig`` (NVS). Kept deliberately
tiny: it remembers the last link you used so reconnecting is one click.
"""
from __future__ import annotations

import json
from dataclasses import asdict, dataclass
from pathlib import Path

_CONFIG_PATH = Path.home() / ".lite_gcs" / "config.json"


@dataclass
class AppConfig:
    connection_type: str = "serial"   # "serial" | "udp" | "tcp"
    serial_port: str = ""
    baud: int = 57600
    udp_port: int = 14550
    tcp_host: str = "127.0.0.1"
    tcp_port: int = 5760

    def connection_string(self) -> str:
        """Build the pymavlink connection string for the selected transport."""
        if self.connection_type == "udp":
            # Listen for whoever streams to us (SITL forward, ESP32 UDP forward,
            # the bundled simulator). mavutil remembers the sender to reply to.
            return f"udpin:0.0.0.0:{self.udp_port}"
        if self.connection_type == "tcp":
            return f"tcp:{self.tcp_host}:{self.tcp_port}"
        return self.serial_port  # serial device path (baud passed separately)

    def label(self) -> str:
        if self.connection_type == "udp":
            return f"udp:{self.udp_port}"
        if self.connection_type == "tcp":
            return f"tcp:{self.tcp_host}:{self.tcp_port}"
        return self.serial_port or "serial"

    # ── persistence ──────────────────────────────────────────────────────────
    @classmethod
    def load(cls) -> "AppConfig":
        try:
            data = json.loads(_CONFIG_PATH.read_text("utf-8"))
            known = {f for f in cls.__dataclass_fields__}  # type: ignore[attr-defined]
            return cls(**{k: v for k, v in data.items() if k in known})
        except Exception:
            return cls()

    def save(self) -> None:
        try:
            _CONFIG_PATH.parent.mkdir(parents=True, exist_ok=True)
            _CONFIG_PATH.write_text(json.dumps(asdict(self), indent=2), "utf-8")
        except Exception:
            pass  # config is best-effort; never block the app on disk errors
