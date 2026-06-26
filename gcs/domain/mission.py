"""Waypoint mission model — the shared source of truth for the 2D and 3D maps.

A mission is an ordered list of waypoints, each a lat/lon plus a relative
altitude (m above home/takeoff). It carries no MAVLink knowledge: the map
widgets edit it, the local simulator flies a marker along it, and (later) a
mission-upload service could translate it to ``MAV_CMD_NAV_WAYPOINT`` items.
Kept as plain data so both the QPainter map and the Cesium WebEngine view render
the exact same points.
"""
from __future__ import annotations

import math
from dataclasses import dataclass, field
from typing import List, Optional, Tuple

_EARTH_R = 6371000.0  # m


def haversine_m(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """Great-circle distance between two lat/lon points, in metres."""
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dphi = math.radians(lat2 - lat1)
    dlmb = math.radians(lon2 - lon1)
    a = (math.sin(dphi / 2) ** 2
         + math.cos(p1) * math.cos(p2) * math.sin(dlmb / 2) ** 2)
    return 2 * _EARTH_R * math.asin(min(1.0, math.sqrt(a)))


def bearing_deg(lat1: float, lon1: float, lat2: float, lon2: float) -> float:
    """Initial compass bearing (deg, 0=N) from point 1 to point 2."""
    p1, p2 = math.radians(lat1), math.radians(lat2)
    dl = math.radians(lon2 - lon1)
    y = math.sin(dl) * math.cos(p2)
    x = math.cos(p1) * math.sin(p2) - math.sin(p1) * math.cos(p2) * math.cos(dl)
    return (math.degrees(math.atan2(y, x)) + 360.0) % 360.0


@dataclass
class Waypoint:
    lat: float
    lon: float
    alt: float = 30.0   # m, relative to home/takeoff

    def as_dict(self) -> dict:
        return {"lat": self.lat, "lon": self.lon, "alt": self.alt}


@dataclass
class Mission:
    """An ordered, mutable list of waypoints with path-geometry helpers."""

    waypoints: List[Waypoint] = field(default_factory=list)
    default_alt: float = 30.0

    # ── mutation ──────────────────────────────────────────────────────────────
    def add(self, lat: float, lon: float, alt: Optional[float] = None) -> Waypoint:
        wp = Waypoint(lat, lon, self.default_alt if alt is None else alt)
        self.waypoints.append(wp)
        return wp

    def move(self, idx: int, lat: float, lon: float) -> None:
        if 0 <= idx < len(self.waypoints):
            self.waypoints[idx].lat = lat
            self.waypoints[idx].lon = lon

    def set_alt(self, idx: int, alt: float) -> None:
        if 0 <= idx < len(self.waypoints):
            self.waypoints[idx].alt = alt

    def remove(self, idx: int) -> None:
        if 0 <= idx < len(self.waypoints):
            self.waypoints.pop(idx)

    def clear(self) -> None:
        self.waypoints.clear()

    # ── queries ───────────────────────────────────────────────────────────────
    def __len__(self) -> int:
        return len(self.waypoints)

    def as_list(self) -> List[dict]:
        return [wp.as_dict() for wp in self.waypoints]

    def total_length_m(self) -> float:
        return sum(seg for _, _, seg in self._segments())

    def _segments(self):
        """Yield ``(wp_a, wp_b, length_m)`` for each leg of the route."""
        wps = self.waypoints
        for a, b in zip(wps, wps[1:]):
            yield a, b, haversine_m(a.lat, a.lon, b.lat, b.lon)

    def interpolate(self, dist_m: float) -> Optional[Tuple[float, float, float, float]]:
        """Position ``dist_m`` along the route as ``(lat, lon, alt, heading)``.

        Returns the first waypoint for ``dist_m <= 0`` and the last for distances
        past the end, or ``None`` when there are fewer than two waypoints. Within
        a leg lat/lon/alt are linearly interpolated (fine at GCS scales) and the
        heading is the leg's bearing.
        """
        wps = self.waypoints
        if len(wps) < 2:
            return None
        if dist_m <= 0:
            a, b = wps[0], wps[1]
            return a.lat, a.lon, a.alt, bearing_deg(a.lat, a.lon, b.lat, b.lon)
        travelled = 0.0
        for a, b, seg in self._segments():
            if seg <= 0:
                continue
            if travelled + seg >= dist_m:
                t = (dist_m - travelled) / seg
                lat = a.lat + (b.lat - a.lat) * t
                lon = a.lon + (b.lon - a.lon) * t
                alt = a.alt + (b.alt - a.alt) * t
                return lat, lon, alt, bearing_deg(a.lat, a.lon, b.lat, b.lon)
            travelled += seg
        last, prev = wps[-1], wps[-2]
        return (last.lat, last.lon, last.alt,
                bearing_deg(prev.lat, prev.lon, last.lat, last.lon))
