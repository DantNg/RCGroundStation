"""Flight-mode tables and a small registry, mirroring ``FlightMode.h``.

MAVLink ``custom_mode`` is autopilot-specific. We resolve the common ArduPilot
Copter table (the typical hobby case) and fall back to a numeric label for
everything else, so the UI always shows *something* sensible.

The registry is open for extension (OCP): add another ``ModeTable`` for a new
vehicle type without touching the lookup code.
"""
from __future__ import annotations

from dataclasses import dataclass
from typing import Dict, Optional

MAV_AUTOPILOT_ARDUPILOTMEGA = 3  # MAV_AUTOPILOT_ARDUPILOTMEGA


@dataclass(frozen=True)
class ModeTable:
    """A bidirectional name<->custom_mode table for one autopilot/vehicle."""

    name: str
    autopilot: int
    by_id: Dict[int, str]

    def name_of(self, custom_mode: int) -> Optional[str]:
        return self.by_id.get(custom_mode)

    def id_of(self, mode_name: str) -> Optional[int]:
        target = mode_name.strip().upper()
        for cid, cname in self.by_id.items():
            if cname == target:
                return cid
        return None


# ── ArduPilot Copter (matches the firmware's FlightMode.h table) ─────────────
ARDUCOPTER = ModeTable(
    name="ArduCopter",
    autopilot=MAV_AUTOPILOT_ARDUPILOTMEGA,
    by_id={
        0: "STABILIZE", 1: "ACRO", 2: "ALT_HOLD", 3: "AUTO", 4: "GUIDED",
        5: "LOITER", 6: "RTL", 7: "CIRCLE", 9: "LAND", 11: "DRIFT",
        13: "SPORT", 14: "FLIP", 15: "AUTOTUNE", 16: "POSHOLD", 17: "BRAKE",
        18: "THROW", 20: "GUIDED_NOGPS", 21: "SMART_RTL", 23: "FOLLOW",
        24: "ZIGZAG", 27: "AUTO_RTL",
    },
)

# Default registry. Today only Copter; add more tables here to extend.
_TABLES = [ARDUCOPTER]


def table_for(autopilot: int) -> Optional[ModeTable]:
    for t in _TABLES:
        if t.autopilot == autopilot:
            return t
    return None


def mode_name(autopilot: int, custom_mode: int) -> str:
    """Best-effort human name; falls back to ``MODE <n>`` like the firmware."""
    table = table_for(autopilot)
    if table is not None:
        name = table.name_of(custom_mode)
        if name is not None:
            return name
    return f"MODE {custom_mode}"


def mode_id(autopilot: int, mode_name_: str) -> Optional[int]:
    """Resolve a mode name to its custom_mode for the given autopilot."""
    table = table_for(autopilot)
    return table.id_of(mode_name_) if table is not None else None


# ── Quick-access modes surfaced as one-tap buttons in the UI ─────────────────
# Order = button order. Mirrors the user's priority: LOITER, STAB, ALTH, LAND.
@dataclass(frozen=True)
class QuickMode:
    label: str       # short button text
    mode_name: str   # canonical mode name used for lookup
    description: str  # tooltip


QUICK_MODES = (
    QuickMode("LOITER", "LOITER", "Hold position (GPS) — pilot can reposition"),
    QuickMode("STAB", "STABILIZE", "Stabilize — manual with self-levelling"),
    QuickMode("ALTH", "ALT_HOLD", "Altitude Hold — hold altitude, manual horizontal"),
    QuickMode("LAND", "LAND", "Land — descend and disarm on touchdown"),
)

# A couple of always-useful extras (not in the priority list but handy).
EXTRA_MODES = (
    QuickMode("RTL", "RTL", "Return To Launch"),
    QuickMode("GUIDED", "GUIDED", "Guided — accept position/velocity targets"),
)
