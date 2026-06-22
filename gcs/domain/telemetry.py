"""Plain value objects describing the vehicle state we display.

These dataclasses carry no behaviour — pure data, copied by value — and are the
contract between the MAVLink decoder (writer) and the UI (readers). They mirror
``src/telemetry/TelemetryTypes.h`` from the ESP32 firmware so the two ports stay
conceptually identical.
"""
from __future__ import annotations

import time
from dataclasses import dataclass, field, replace
from enum import IntEnum


def _now_ms() -> int:
    """Monotonic milliseconds — the desktop analogue of Arduino ``millis()``."""
    return int(time.monotonic() * 1000.0)


class Severity(IntEnum):
    """STATUSTEXT severity (MAV_SEVERITY), lowest value = most severe."""

    EMERGENCY = 0
    ALERT = 1
    CRITICAL = 2
    ERROR = 3
    WARNING = 4
    NOTICE = 5
    INFO = 6
    DEBUG = 7

    @property
    def label(self) -> str:
        return self.name.capitalize()


@dataclass
class Attitude:
    """From ATTITUDE (radians)."""

    roll: float = 0.0
    pitch: float = 0.0
    yaw: float = 0.0
    updated_ms: int = 0


@dataclass
class Battery:
    """From SYS_STATUS / BATTERY_STATUS."""

    voltage: float = 0.0       # V
    current: float = 0.0       # A
    remaining: int = -1        # %, -1 = unknown
    updated_ms: int = 0


@dataclass
class GeoPosition:
    """From GLOBAL_POSITION_INT (+ GPS_RAW_INT for fix quality)."""

    lat: float = 0.0           # deg
    lon: float = 0.0           # deg
    alt_msl: float = 0.0       # m
    alt_rel: float = 0.0       # m
    heading_deg: float = 0.0   # deg
    valid: bool = False
    updated_ms: int = 0


@dataclass
class Vfr:
    """Air data from VFR_HUD."""

    airspeed: float = 0.0      # m/s
    groundspeed: float = 0.0   # m/s
    climb: float = 0.0         # m/s
    throttle: int = 0          # %
    updated_ms: int = 0


@dataclass
class GpsInfo:
    """GPS link quality from GPS_RAW_INT."""

    fix_type: int = 0          # 0-1 none, 2 = 2D, 3 = 3D, ...
    satellites: int = 0
    hdop: float = 0.0
    updated_ms: int = 0

    @property
    def fix_label(self) -> str:
        return {0: "No GPS", 1: "No Fix", 2: "2D", 3: "3D",
                4: "DGPS", 5: "RTK Float", 6: "RTK Fixed"}.get(self.fix_type,
                                                               f"Fix {self.fix_type}")


@dataclass
class FlightModeInfo:
    """Mode / arming from HEARTBEAT."""

    base_mode: int = 0
    custom_mode: int = 0
    mav_type: int = 0          # MAV_TYPE_*
    autopilot: int = 0         # MAV_AUTOPILOT_*
    system_status: int = 0     # MAV_STATE_*
    armed: bool = False
    updated_ms: int = 0


@dataclass
class LinkStats:
    """Receive-side link health (maintained by the link worker)."""

    frames_received: int = 0
    bytes_received: int = 0
    parse_errors: int = 0
    last_frame_ms: int = 0
    link_up: bool = False
    source_name: str = "—"


@dataclass
class StatusText:
    """A single STATUSTEXT line (used for the Mission-Planner-style message log)."""

    severity: Severity = Severity.INFO
    text: str = ""
    updated_ms: int = 0
    valid: bool = False


@dataclass
class TelemetrySnapshot:
    """Full snapshot copied atomically out of the TelemetryStore."""

    attitude: Attitude = field(default_factory=Attitude)
    battery: Battery = field(default_factory=Battery)
    position: GeoPosition = field(default_factory=GeoPosition)
    gps: GpsInfo = field(default_factory=GpsInfo)
    vfr: Vfr = field(default_factory=Vfr)
    mode: FlightModeInfo = field(default_factory=FlightModeInfo)
    link: LinkStats = field(default_factory=LinkStats)
    status: StatusText = field(default_factory=StatusText)
    heartbeat_seen: bool = False
    last_heartbeat_ms: int = 0

    def copy(self) -> "TelemetrySnapshot":
        """Deep-ish copy (each member dataclass is shallow-copied)."""
        return replace(
            self,
            attitude=replace(self.attitude),
            battery=replace(self.battery),
            position=replace(self.position),
            gps=replace(self.gps),
            vfr=replace(self.vfr),
            mode=replace(self.mode),
            link=replace(self.link),
            status=replace(self.status),
        )
