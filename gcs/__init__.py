"""Lite Ground Station — desktop edition.

A cross-platform (Windows / Linux) port of the ESP32 CrowPanel ground station.
It receives & decodes MAVLink telemetry, shows a Mission-Planner-style HUD and
map, and — unlike the embedded version — can *command* the vehicle (arm/disarm
and quick flight-mode changes).

The package is organised around SOLID principles:

    domain/      pure data + flight-mode tables (no behaviour, no I/O)
    interfaces/  abstract ports (ITelemetryLink, ICommandSink) — DIP boundary
    mavlink/     concrete pymavlink adapters for those ports
    app/         thread-safe store, link worker, composition root
    ui/          PySide6 widgets (HUD, telemetry, control, messages, map)
"""

__version__ = "1.0.0"
