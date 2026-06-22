"""Modern, fully responsive PFD-style HUD (QPainter).

Every dimension derives from a single ``scale`` factor (``height / 480``), so the
instrument looks proportional and professional at any size — from a wide desktop
pane down to a 4.3" 480×272 panel. Below a small breakpoint the side tapes drop
away and a compact readout takes over.

Modern touches vs. the firmware port: gradient sky/ground that rotates with the
horizon, a sampled roll arc with a sky pointer, notched EFIS value boxes,
monospace digits, and translucent corner data chips (mode / arming / battery /
GPS / link) so the HUD alone is informative on a tiny screen.
"""
from __future__ import annotations

import math

from PySide6.QtCore import QPointF, QRectF, Qt
from PySide6.QtGui import (QBrush, QColor, QFont, QFontMetrics, QLinearGradient,
                           QPainter, QPen, QPolygonF)
from PySide6.QtWidgets import QWidget

from ..domain import flight_modes
from ..domain.telemetry import TelemetrySnapshot
from . import theme

R2D = 57.29577951


def _clamp(v, lo, hi):
    return lo if v < lo else hi if v > hi else v


class HudWidget(QWidget):
    def __init__(self, parent=None):
        super().__init__(parent)
        self.setMinimumSize(200, 150)
        # attitude / air data
        self._roll = self._pitch = self._heading = 0.0
        self._alt = self._airspeed = self._groundspeed = self._climb = 0.0
        self._valid = False
        # for the corner chips
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
        self._groundspeed = s.vfr.groundspeed
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
    def paintEvent(self, _event) -> None:
        p = QPainter(self)
        p.setRenderHint(QPainter.Antialiasing, True)
        p.setRenderHint(QPainter.TextAntialiasing, True)
        w, h = self.width(), self.height()
        s = _clamp(h / 480.0, 0.5, 2.2)
        g = {
            "w": w, "h": h, "s": s,
            "cx": w / 2.0, "cy": h / 2.0,
            "px_per_deg": h * 0.0092,
            "tw": int(_clamp(48 * s, 34, 66)),
            "ribbon_h": int(_clamp(26 * s, 16, 34)),
            "tapes": (w >= 300 and h >= 200),
            "f_small": int(_clamp(round(9 * s), 7, 17)),
            "f_med": int(_clamp(round(12 * s), 9, 22)),
            "f_big": int(_clamp(round(16 * s), 12, 30)),
            "line_w": max(1, round(2 * s)),
        }
        self._g = g

        self._draw_horizon(p, g)
        self._draw_pitch_ladder(p, g)
        self._draw_roll_arc(p, g)
        self._draw_boresight(p, g)
        if g["tapes"]:
            self._draw_tapes(p, g)
        else:
            self._draw_compact(p, g)
        self._draw_corner_chips(p, g)
        if not self._valid:
            self._draw_no_data(p, g)
        p.end()

    # ── horizon (gradient sky / ground that rotates with bank) ────────────────
    def _draw_horizon(self, p, g) -> None:
        cx, cy = g["cx"], g["cy"]
        w, h = g["w"], g["h"]
        a = -self._roll
        ca, sa = math.cos(a), math.sin(a)
        ux, uy = ca, sa            # along the horizon
        nx, ny = -sa, ca           # perpendicular, points to ground
        pitch_deg = self._pitch * R2D
        ccx = cx + nx * pitch_deg * g["px_per_deg"]
        ccy = cy + ny * pitch_deg * g["px_per_deg"]
        L = (w + h) * 1.8
        D = (w + h) * 2.2
        self._ccx, self._ccy = ccx, ccy
        self._ux, self._uy, self._nx, self._ny = ux, uy, nx, ny

        def quad(off1, off2, brush):
            p.setPen(Qt.NoPen)
            p.setBrush(brush)
            p.drawPolygon(QPolygonF([
                QPointF(ccx - ux * L + nx * off1, ccy - uy * L + ny * off1),
                QPointF(ccx + ux * L + nx * off1, ccy + uy * L + ny * off1),
                QPointF(ccx + ux * L + nx * off2, ccy + uy * L + ny * off2),
                QPointF(ccx - ux * L + nx * off2, ccy - uy * L + ny * off2),
            ]))

        span = h * 1.1
        sky = QLinearGradient(QPointF(ccx - nx * span, ccy - ny * span),
                              QPointF(ccx, ccy))
        sky.setColorAt(0.0, theme.HUD_SKY_TOP)
        sky.setColorAt(1.0, theme.HUD_SKY_HORIZON)
        ground = QLinearGradient(QPointF(ccx, ccy),
                                 QPointF(ccx + nx * span, ccy + ny * span))
        ground.setColorAt(0.0, theme.HUD_GROUND_HORIZON)
        ground.setColorAt(1.0, theme.HUD_GROUND_BOTTOM)
        quad(-D, 0, QBrush(sky))
        quad(0, D, QBrush(ground))

        p.setPen(QPen(theme.HUD_LINE, g["line_w"]))
        p.drawLine(QPointF(ccx - ux * L, ccy - uy * L),
                   QPointF(ccx + ux * L, ccy + uy * L))

    def _draw_pitch_ladder(self, p, g) -> None:
        ccx, ccy = self._ccx, self._ccy
        ux, uy, nx, ny = self._ux, self._uy, self._nx, self._ny
        ppd = g["px_per_deg"]
        s = g["s"]
        half10 = 34 * s
        half5 = 18 * s
        gap = 9 * s
        p.setFont(self._font(g["f_small"]))
        for m in range(-30, 31, 5):
            if m == 0:
                continue
            off = -m * ppd
            mx, my = ccx + nx * off, ccy + ny * off
            major = (m % 10 == 0)
            half = half10 if major else half5
            pen = QPen(theme.HUD_LINE, max(1, round((2 if major else 1) * s)))
            if m < 0:
                pen.setStyle(Qt.DashLine)
            p.setPen(pen)
            p.drawLine(QPointF(mx - ux * half, my - uy * half),
                       QPointF(mx - ux * gap, my - uy * gap))
            p.drawLine(QPointF(mx + ux * gap, my + uy * gap),
                       QPointF(mx + ux * half, my + uy * half))
            if major:
                p.setPen(QPen(theme.HUD_LINE))
                lbl = str(abs(m))
                p.drawText(QRectF(mx + ux * half + 4 * s, my - 9 * s, 30 * s, 18 * s),
                           Qt.AlignVCenter | Qt.AlignLeft, lbl)
                p.drawText(QRectF(mx - ux * half - 34 * s, my - 9 * s, 30 * s, 18 * s),
                           Qt.AlignVCenter | Qt.AlignRight, lbl)

    def _draw_roll_arc(self, p, g) -> None:
        cx, cy, s = g["cx"], g["cy"], g["s"]
        r = g["h"] * 0.40
        # arc line (sampled to match the tick geometry exactly)
        p.setPen(QPen(QColor(255, 255, 255, 180), max(1, round(1.5 * s))))
        pts = [QPointF(cx + math.cos(math.radians(-90 + d)) * r,
                       cy + math.sin(math.radians(-90 + d)) * r)
               for d in range(-60, 61, 3)]
        p.drawPolyline(QPolygonF(pts))
        # ticks
        for t in (-60, -45, -30, -20, -10, 0, 10, 20, 30, 45, 60):
            ang = math.radians(-90 + t)
            co, si = math.cos(ang), math.sin(ang)
            ln = (10 if t == 0 else 8 if t % 30 == 0 else 5) * s
            p.setPen(QPen(theme.HUD_LINE, max(1, round((2 if t % 30 == 0 else 1) * s))))
            p.drawLine(QPointF(cx + co * r, cy + si * r),
                       QPointF(cx + co * (r + ln), cy + si * (r + ln)))
        # fixed reference triangle (sky pointer) at top
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(theme.HUD_LINE))
        tp = 7 * s
        p.drawPolygon(QPolygonF([
            QPointF(cx, cy - r),
            QPointF(cx - tp, cy - r - tp * 1.6),
            QPointF(cx + tp, cy - r - tp * 1.6)]))
        # moving bank pointer (amber) riding the arc
        ang = math.radians(-90 + self._roll * R2D)
        co, si = math.cos(ang), math.sin(ang)
        px, py = -si, co
        ap = 6 * s
        p.setBrush(QBrush(theme.HUD_ACCENT))
        p.drawPolygon(QPolygonF([
            QPointF(cx + co * (r - 1), cy + si * (r - 1)),
            QPointF(cx + co * (r - ap * 2) + px * ap, cy + si * (r - ap * 2) + py * ap),
            QPointF(cx + co * (r - ap * 2) - px * ap, cy + si * (r - ap * 2) - py * ap)]))

    def _draw_boresight(self, p, g) -> None:
        cx, cy, s = g["cx"], g["cy"], g["s"]
        ww, wi, drop = 48 * s, 18 * s, 9 * s
        pen = QPen(theme.HUD_ACCENT, max(2, round(3.5 * s)))
        pen.setCapStyle(Qt.RoundCap)
        pen.setJoinStyle(Qt.RoundJoin)
        p.setPen(pen)
        p.setBrush(Qt.NoBrush)
        p.drawPolyline(QPolygonF([
            QPointF(cx - ww, cy), QPointF(cx - wi, cy), QPointF(cx - wi, cy + drop)]))
        p.drawPolyline(QPolygonF([
            QPointF(cx + ww, cy), QPointF(cx + wi, cy), QPointF(cx + wi, cy + drop)]))
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(theme.HUD_ACCENT))
        p.drawEllipse(QPointF(cx, cy), max(2, 3 * s), max(2, 3 * s))

    # ── tapes ──────────────────────────────────────────────────────────────
    def _draw_tapes(self, p, g) -> None:
        w, h, s = g["w"], g["h"], g["s"]
        cx = g["cx"]
        tw, ribbon = g["tw"], g["ribbon_h"]
        top = ribbon + int(10 * s)
        bot = h - int(12 * s)
        tcy = (top + bot) / 2.0
        digit = self._font(g["f_med"], mono=True)
        small = self._font(g["f_small"], mono=True)

        # airspeed (left)
        p.fillRect(QRectF(0, top, tw, bot - top), theme.HUD_TAPE_BG)
        spp = 6.5 * s
        self._tape_ticks(p, g, 0, tw, tcy, self._airspeed, spp, step=5, major=10,
                         right_side=True, label_min=0, font=small)
        self._value_box(p, QRectF(0, tcy - 14 * s, tw + 16 * s, 28 * s),
                        f"{self._airspeed:.0f}", digit, "right")

        # altitude (right)
        x1 = w - tw
        p.fillRect(QRectF(x1, top, tw, bot - top), theme.HUD_TAPE_BG)
        app = 2.4 * s
        self._tape_ticks(p, g, x1, w, tcy, self._alt, app, step=10, major=50,
                         right_side=False, label_min=None, font=small)
        self._value_box(p, QRectF(x1 - 16 * s, tcy - 14 * s, tw + 16 * s, 28 * s),
                        f"{self._alt:.0f}", digit, "left")
        # climb under the altitude box
        p.setFont(small)
        p.setPen(QPen(theme.HUD_TEXT if self._climb >= 0 else theme.HUD_ACCENT))
        p.drawText(QRectF(x1 - 16 * s, tcy + 16 * s, tw + 16 * s, 16 * s),
                   Qt.AlignCenter, f"{self._climb:+.1f}")

        # heading ribbon (top)
        p.fillRect(QRectF(0, 0, w, ribbon), theme.HUD_TAPE_BG)
        hpp = w * 0.011
        rng = w / 2.0 / hpp
        h0 = int(math.floor((self._heading - rng) / 10.0)) * 10
        h1 = int(math.ceil((self._heading + rng) / 10.0)) * 10
        p.setFont(small)
        for d in range(h0, h1 + 1, 10):
            x = cx + (d - self._heading) * hpp
            if x < 4 or x > w - 4:
                continue
            dn = ((d % 360) + 360) % 360
            major = (dn % 30 == 0)
            p.setPen(QPen(theme.HUD_TAPE_TICK, max(1, round(s))))
            p.drawLine(QPointF(x, ribbon - (10 if major else 6) * s), QPointF(x, ribbon - 2 * s))
            if major:
                card = {0: "N", 90: "E", 180: "S", 270: "W"}.get(dn)
                p.setPen(QPen(theme.HUD_TEXT if card else theme.HUD_TAPE_TICK))
                p.drawText(QRectF(x - 14 * s, 0, 28 * s, ribbon - 8 * s),
                           Qt.AlignCenter, card or str(dn // 10))
        self._value_box(p, QRectF(cx - 26 * s, 0, 52 * s, ribbon + 7 * s),
                        f"{int(round(self._heading)) % 360:03d}", digit, "down")

    def _tape_ticks(self, p, g, x_left, x_right, tcy, value, ppu, step, major,
                    right_side, label_min, font) -> None:
        s = g["s"]
        top = g["ribbon_h"] + int(10 * s)
        bot = g["h"] - int(12 * s)
        rng = (bot - top) / 2.0 / ppu
        v0 = int(math.floor((value - rng) / step)) * step
        v1 = int(math.ceil((value + rng) / step)) * step
        p.setFont(font)
        for v in range(v0, v1 + 1, step):
            if label_min is not None and v < label_min:
                continue
            y = tcy - (v - value) * ppu
            if y < top + 4 or y > bot - 4:
                continue
            is_major = (v % major == 0)
            p.setPen(QPen(theme.HUD_TAPE_TICK, max(1, round(s))))
            tl = (12 if is_major else 7) * s
            if right_side:
                p.drawLine(QPointF(x_right - tl, y), QPointF(x_right - 2 * s, y))
                if is_major:
                    p.drawText(QRectF(x_left + 3 * s, y - 8 * s, (x_right - x_left) - 16 * s, 16 * s),
                               Qt.AlignVCenter | Qt.AlignLeft, str(v))
            else:
                p.drawLine(QPointF(x_left + 2 * s, y), QPointF(x_left + tl, y))
                if is_major:
                    p.drawText(QRectF(x_left + 15 * s, y - 8 * s, (x_right - x_left) - 16 * s, 16 * s),
                               Qt.AlignVCenter | Qt.AlignLeft, str(v))

    def _value_box(self, p, rect: QRectF, text: str, font, notch: str) -> None:
        s = self._g["s"]
        p.setPen(QPen(theme.HUD_CYAN, max(1, round(s))))
        p.setBrush(QBrush(theme.HUD_BOX_BG))
        p.drawRoundedRect(rect, 3 * s, 3 * s)
        n = 5 * s
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(theme.HUD_BOX_BG))
        cyf = rect.center().y()
        if notch == "right":
            p.drawPolygon(QPolygonF([QPointF(rect.right(), cyf - n),
                                     QPointF(rect.right() + n, cyf),
                                     QPointF(rect.right(), cyf + n)]))
        elif notch == "left":
            p.drawPolygon(QPolygonF([QPointF(rect.left(), cyf - n),
                                     QPointF(rect.left() - n, cyf),
                                     QPointF(rect.left(), cyf + n)]))
        elif notch == "down":
            cxf = rect.center().x()
            p.drawPolygon(QPolygonF([QPointF(cxf - n, rect.bottom()),
                                     QPointF(cxf, rect.bottom() + n),
                                     QPointF(cxf + n, rect.bottom())]))
        p.setPen(QPen(theme.HUD_TEXT))
        p.setFont(font)
        p.drawText(rect, Qt.AlignCenter, text)

    def _draw_compact(self, p, g) -> None:
        cx, cy, s, w = g["cx"], g["cy"], g["s"], g["w"]
        digit = self._font(g["f_med"], mono=True)
        bw, bh = 46 * s, 24 * s
        self._value_box(p, QRectF(4 * s, cy - bh / 2, bw, bh), f"{self._airspeed:.0f}",
                        digit, "right")
        self._value_box(p, QRectF(w - bw - 4 * s, cy - bh / 2, bw, bh), f"{self._alt:.0f}",
                        digit, "left")
        self._value_box(p, QRectF(cx - bw / 2, 4 * s, bw, bh),
                        f"{int(round(self._heading)) % 360:03d}", digit, "down")

    # ── corner data chips ─────────────────────────────────────────────────────
    def _draw_corner_chips(self, p, g) -> None:
        s = g["s"]
        inset = (g["tw"] if g["tapes"] else int(6 * s)) + int(6 * s)
        top = g["ribbon_h"] + int(6 * s)
        f_med = self._font(g["f_med"])
        f_small = self._font(g["f_small"])

        # left chip — flight mode + arming
        arm_txt = "ARMED" if self._armed else "DISARMED"
        arm_col = theme.BAD if self._armed else theme.GOOD
        self._chip(p, inset, top, "left", s, [
            (self._mode, theme.HUD_CYAN, f_med),
            (arm_txt, QColor(arm_col), f_small),
        ])

        # right chip — battery / GPS / link
        batt = f"{self._batt_v:.1f}V" + (f" {self._batt_pct}%" if self._batt_pct >= 0 else "")
        if self._batt_pct < 0:
            bcol = theme.HUD_TEXT_DIM
        elif self._batt_pct < 20:
            bcol = QColor(theme.BAD)
        elif self._batt_pct < 40:
            bcol = QColor(theme.WARN)
        else:
            bcol = theme.HUD_TEXT
        gps_txt = f"{self._fix_label()} · {self._gps_sats} sat"
        gcol = theme.GOOD if self._gps_fix >= 3 else (theme.WARN if self._gps_fix == 2 else theme.BAD)
        self._chip(p, g["w"] - inset, top, "right", s, [
            (batt, bcol, f_small),
            (gps_txt, QColor(gcol), f_small),
            ("LINK" if self._link_up else "NO LINK",
             QColor(theme.GOOD if self._link_up else theme.BAD), f_small),
        ])

    def _chip(self, p, anchor_x, y, align, s, lines) -> None:
        pad = 6 * s
        line_h = 0
        width = 0
        for text, _col, font in lines:
            fm = QFontMetrics(font)
            width = max(width, fm.horizontalAdvance(text))
            line_h = max(line_h, fm.height())
        box_w = width + pad * 2
        box_h = line_h * len(lines) + pad * 1.4
        x = anchor_x if align == "left" else anchor_x - box_w
        p.setPen(Qt.NoPen)
        p.setBrush(QBrush(theme.HUD_CHIP_BG))
        p.drawRoundedRect(QRectF(x, y, box_w, box_h), 4 * s, 4 * s)
        cy = y + pad * 0.6
        for text, col, font in lines:
            p.setFont(font)
            p.setPen(QPen(col))
            p.drawText(QRectF(x + pad, cy, width, line_h), Qt.AlignVCenter | Qt.AlignLeft, text)
            cy += line_h

    def _draw_no_data(self, p, g) -> None:
        p.fillRect(self.rect(), QColor(0, 0, 0, 130))
        p.setPen(QPen(theme.HUD_TEXT_DIM))
        p.setFont(self._font(g["f_big"], bold=True))
        p.drawText(self.rect(), Qt.AlignCenter, "NO TELEMETRY")

    # ── helpers ───────────────────────────────────────────────────────────────
    def _fix_label(self) -> str:
        return {0: "No GPS", 1: "NoFix", 2: "2D", 3: "3D",
                4: "DGPS", 5: "RTKf", 6: "RTK"}.get(self._gps_fix, f"F{self._gps_fix}")

    @staticmethod
    def _font(px: int, mono: bool = False, bold: bool = False) -> QFont:
        family = "Consolas" if mono else "Segoe UI"
        f = QFont(family, px)
        f.setPixelSize(px)
        if bold:
            f.setBold(True)
        return f
