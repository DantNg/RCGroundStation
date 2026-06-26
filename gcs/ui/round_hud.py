"""Compact circular attitude gauge (a round PFD "ball").

Designed to float over the map in the autonomous fly-view: small, glanceable,
self-contained. It clips the gradient horizon to a circle, wraps it in a bezel
with a roll scale, and shows only the IMU/attitude picture — roll & pitch from
the ball, yaw in the heading box, plus a single altitude readout below. The
remaining vehicle status (mode, arming, battery, GPS, link) lives in the top
status bar, so this instrument stays purely an artificial horizon.
"""
from __future__ import annotations

import math

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import (QBrush, QColor, QFont, QLinearGradient, QPainter,
                           QPainterPath, QPen, QPolygonF)
from PySide6.QtWidgets import QWidget

from ..domain import flight_modes
from ..domain.telemetry import TelemetrySnapshot
from . import theme

R2D = 57.29577951


def _clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


class RoundHud(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(80, 96)   # may shrink on small panels; sizing is geometry-driven
        # Translucent floating card: WA_TranslucentBackground makes Qt clear the
        # widget every frame and composite over the map — without it a child with
        # an unpainted (transparent) background smears and the horizon "freezes".
        # The card itself is painted fully opaque, so even on a backend that
        # ignores the attribute the instrument never smears.
        self.setAttribute(Qt.WA_TranslucentBackground, True)
        self._roll = self._pitch = self._heading = 0.0
        self._alt = self._airspeed = self._climb = 0.0
        self._valid = False
        self._mode = "—"
        self._armed = False
        self._batt_v = 0.0
        self._batt_pct = -1
        self._gps_fix = 0
        self._gps_sats = 0
        self._link_up = False

    def update_from(self, s: TelemetrySnapshot) -> None:
        self._roll = s.attitude.roll
        self._pitch = s.attitude.pitch
        self._heading = s.position.heading_deg
        self._alt = s.position.alt_rel
        self._airspeed = s.vfr.airspeed
        self._climb = s.vfr.climb
        self._valid = s.heartbeat_seen
        self._mode = flight_modes.mode_name(s.mode.autopilot, s.mode.custom_mode) \
            if s.heartbeat_seen else "—"
        self._armed = s.mode.armed
        self._batt_v = s.battery.voltage
        self._batt_pct = s.battery.remaining
        self._gps_fix = s.gps.fix_type
        self._gps_sats = s.gps.satellites
        self._link_up = s.link.link_up
        self.update()

    # ── painting ──────────────────────────────────────────────────────────────
    def paintEvent(self, _e) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        p.setRenderHint(QPainter.TextAntialiasing, True)
        w, h = self.width(), self.height()
        s = _clamp(min(w, h) / 220.0, 0.42, 1.6)

        # opaque rounded card (covers the whole body; only the rounded corners
        # stay transparent so the map shows through)
        p.setPen(QPen(QColor(theme.PANEL_BORDER), 1))
        p.setBrush(QBrush(QColor(13, 17, 23)))
        p.drawRoundedRect(QRectF(0.5, 0.5, w - 1, h - 1), 10 * s, 10 * s)

        line_h = int(16 * s)
        strip_h = line_h + int(8 * s)   # one line: altitude only
        avail = h - strip_h
        R = max(20.0, min(w, avail) / 2.0 - 4 * s)
        cx = w / 2.0
        cy = 4 * s + R

        self._draw_ball(p, cx, cy, R, s)
        self._draw_bezel_and_roll(p, cx, cy, R, s)
        self._draw_boresight(p, cx, cy, s)
        self._draw_heading_box(p, cx, cy, R, s)
        self._draw_alt(p, w, h - strip_h, strip_h, s)
        if not self._valid:
            self._draw_no_data(p, cx, cy, R, s)
        p.end()

    def _draw_ball(self, p, cx, cy, R, s) -> None:
        p.save()
        clip = QPainterPath()
        clip.addEllipse(QPointF(cx, cy), R, R)
        p.setClipPath(clip)

        a = -self._roll
        ca, sa = math.cos(a), math.sin(a)
        ux, uy = ca, sa
        nx, ny = -sa, ca
        ppd = R / 26.0
        pitch_deg = self._pitch * R2D
        ccx = cx + nx * pitch_deg * ppd
        ccy = cy + ny * pitch_deg * ppd
        L = R * 3.0
        D = R * 3.0

        def quad(off1, off2, brush):
            p.setPen(Qt.NoPen)
            p.setBrush(brush)
            p.drawPolygon(QPolygonF([
                QPointF(ccx - ux * L + nx * off1, ccy - uy * L + ny * off1),
                QPointF(ccx + ux * L + nx * off1, ccy + uy * L + ny * off1),
                QPointF(ccx + ux * L + nx * off2, ccy + uy * L + ny * off2),
                QPointF(ccx - ux * L + nx * off2, ccy - uy * L + ny * off2),
            ]))

        span = R * 2.0
        sky = QLinearGradient(QPointF(ccx - nx * span, ccy - ny * span), QPointF(ccx, ccy))
        sky.setColorAt(0.0, theme.HUD_SKY_TOP)
        sky.setColorAt(1.0, theme.HUD_SKY_HORIZON)
        gnd = QLinearGradient(QPointF(ccx, ccy), QPointF(ccx + nx * span, ccy + ny * span))
        gnd.setColorAt(0.0, theme.HUD_GROUND_HORIZON)
        gnd.setColorAt(1.0, theme.HUD_GROUND_BOTTOM)
        quad(-D, 0, QBrush(sky))
        quad(0, D, QBrush(gnd))

        # horizon line
        p.setPen(QPen(theme.HUD_LINE, max(1, round(2 * s))))
        p.drawLine(QPointF(ccx - ux * L, ccy - uy * L), QPointF(ccx + ux * L, ccy + uy * L))

        # pitch ladder (±20°, labelled at 10/20)
        p.setFont(QFont("Segoe UI", max(6, int(8 * s))))
        for m in range(-20, 21, 10):
            if m == 0:
                continue
            off = -m * ppd
            mx, my = ccx + nx * off, ccy + ny * off
            half = 16 * s
            p.setPen(QPen(theme.HUD_LINE, max(1, round(1.4 * s))))
            p.drawLine(QPointF(mx - ux * half, my - uy * half),
                       QPointF(mx + ux * half, my + uy * half))
        p.restore()

    def _draw_bezel_and_roll(self, p, cx, cy, R, s) -> None:
        # bezel ring
        p.setPen(QPen(QColor("#0b0f14"), max(2, round(3 * s))))
        p.setBrush(Qt.NoBrush)
        p.drawEllipse(QPointF(cx, cy), R, R)
        p.setPen(QPen(QColor(255, 255, 255, 60), max(1, round(s))))
        p.drawEllipse(QPointF(cx, cy), R, R)

        # roll ticks on the rim (top half)
        for t in (-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60):
            ang = math.radians(-90 + t)
            co, si = math.cos(ang), math.sin(ang)
            ln = (8 if t % 30 == 0 else 5) * s
            p.setPen(QPen(theme.HUD_LINE, max(1, round((2 if t % 30 == 0 else 1) * s))))
            p.drawLine(QPointF(cx + co * (R - ln), cy + si * (R - ln)),
                       QPointF(cx + co * R, cy + si * R))
        # fixed reference triangle above the top of the ring
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(theme.HUD_LINE))
        tp = 6 * s
        p.drawPolygon(QPolygonF([
            QPointF(cx, cy - R + 1),
            QPointF(cx - tp, cy - R + tp * 1.6),
            QPointF(cx + tp, cy - R + tp * 1.6)]))
        # moving bank pointer (amber)
        ang = math.radians(-90 + self._roll * R2D)
        co, si = math.cos(ang), math.sin(ang)
        px, py = -si, co
        ap = 6 * s
        p.setBrush(QBrush(theme.HUD_ACCENT))
        p.drawPolygon(QPolygonF([
            QPointF(cx + co * (R - 1), cy + si * (R - 1)),
            QPointF(cx + co * (R - ap * 2) + px * ap, cy + si * (R - ap * 2) + py * ap),
            QPointF(cx + co * (R - ap * 2) - px * ap, cy + si * (R - ap * 2) - py * ap)]))

    def _draw_boresight(self, p, cx, cy, s) -> None:
        ww, wi, drop = 26 * s, 9 * s, 6 * s
        pen = QPen(theme.HUD_ACCENT, max(2, round(3 * s)))
        pen.setCapStyle(Qt.RoundCap)
        pen.setJoinStyle(Qt.RoundJoin)
        p.setPen(pen)
        p.setBrush(Qt.NoBrush)
        p.drawPolyline(QPolygonF([QPointF(cx - ww, cy), QPointF(cx - wi, cy),
                                  QPointF(cx - wi, cy + drop)]))
        p.drawPolyline(QPolygonF([QPointF(cx + ww, cy), QPointF(cx + wi, cy),
                                  QPointF(cx + wi, cy + drop)]))
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(theme.HUD_ACCENT))
        p.drawEllipse(QPointF(cx, cy), max(2, 2.5 * s), max(2, 2.5 * s))

    def _draw_heading_box(self, p, cx, cy, R, s) -> None:
        bw, bh = 42 * s, 18 * s
        rect = QRectF(cx - bw / 2, cy - R - bh - 4 * s, bw, bh)
        p.setPen(QPen(theme.HUD_CYAN, max(1, round(s))))
        p.setBrush(QBrush(theme.HUD_BOX_BG))
        p.drawRoundedRect(rect, 3 * s, 3 * s)
        p.setPen(QPen(theme.HUD_TEXT))
        hf = QFont("Consolas")
        hf.setPixelSize(max(7, int(bh * 0.72)))
        p.setFont(hf)
        p.drawText(rect, Qt.AlignCenter, f"{int(round(self._heading)) % 360:03d}")

    def _draw_alt(self, p, w, y, strip_h, s) -> None:
        """A single altitude readout under the ball (the only non-IMU value)."""
        f = QFont("Consolas")
        f.setBold(True)
        f.setPixelSize(max(9, int(strip_h * 0.62)))
        p.setFont(f)
        p.setPen(QPen(theme.HUD_TEXT_DIM))
        p.drawText(QRectF(0, y, w, strip_h), Qt.AlignCenter,
                   f"ALT  {self._alt:.0f} m")

    def _draw_no_data(self, p, cx, cy, R, s) -> None:
        p.setBrush(QBrush(QColor(0, 0, 0, 120)))
        p.setPen(Qt.NoPen)
        p.drawEllipse(QPointF(cx, cy), R, R)
        p.setPen(QPen(theme.HUD_TEXT_DIM))
        p.setFont(QFont("Segoe UI", max(8, int(10 * s)), QFont.Bold))
        p.drawText(QRectF(cx - R, cy - 10 * s, 2 * R, 20 * s), Qt.AlignCenter, "NO DATA")

    def _fix_label(self) -> str:
        return {0: "NoGPS", 1: "NoFix", 2: "2D", 3: "3D",
                4: "DGPS", 5: "RTKf", 6: "RTK"}.get(self._gps_fix, f"F{self._gps_fix}")
