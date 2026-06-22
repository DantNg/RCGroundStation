"""Pure data layer: telemetry value objects and flight-mode tables.

This layer has no dependencies on Qt, pymavlink, threads or I/O. It is the
shared vocabulary between the decoder (writer) and the UI (readers), mirroring
the ESP32 firmware's ``telemetry/TelemetryTypes.h``.
"""
