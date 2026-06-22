"""Centralised colours and a dark stylesheet (one place to restyle the app).

Palette echoes the firmware's GitHub-dark UI plus a Mission-Planner-style HUD
(blue sky / brown ground / white lines / amber aircraft symbol).
"""
from __future__ import annotations

from PySide6.QtGui import QColor

from ..domain.telemetry import Severity

# ── general UI ──────────────────────────────────────────────────────────────
BG = "#0d1117"
PANEL = "#161b22"
PANEL_BORDER = "#30363d"
TEXT = "#c9d1d9"
TEXT_DIM = "#8b949e"
ACCENT = "#ffb000"        # amber — aircraft symbol, active highlights
GOOD = "#3fb950"          # green — connected / disarmed-safe
BAD = "#f85149"           # red — error / armed / disconnected
WARN = "#d29922"          # amber — warnings

# ── HUD (modern PFD/EFIS palette) ────────────────────────────────────────────
# Sky and ground are drawn as vertical gradients (deep → bright at the horizon).
HUD_SKY_TOP = QColor("#0b3a66")
HUD_SKY_HORIZON = QColor("#2f8fd6")
HUD_GROUND_HORIZON = QColor("#a06a30")
HUD_GROUND_BOTTOM = QColor("#3c2913")
HUD_LINE = QColor("#eef4f9")          # near-white horizon / ladder lines
HUD_ACCENT = QColor(ACCENT)            # amber aircraft boresight
HUD_CYAN = QColor("#39e1ff")           # cyan bug / selected pointer
HUD_TAPE_BG = QColor(8, 12, 18, 175)
HUD_TAPE_TICK = QColor("#cdd9e3")
HUD_BOX_BG = QColor(8, 12, 18, 225)
HUD_CHIP_BG = QColor(8, 12, 18, 160)
HUD_TEXT = QColor("#ffffff")
HUD_TEXT_DIM = QColor("#9fb1bf")

# ── severity → colour for the message log ───────────────────────────────────
_SEVERITY_COLOR = {
    Severity.EMERGENCY: BAD,
    Severity.ALERT: BAD,
    Severity.CRITICAL: BAD,
    Severity.ERROR: BAD,
    Severity.WARNING: WARN,
    Severity.NOTICE: "#58a6ff",
    Severity.INFO: TEXT,
    Severity.DEBUG: TEXT_DIM,
}


def severity_color(sev: Severity) -> str:
    return _SEVERITY_COLOR.get(sev, TEXT)


STYLESHEET = f"""
QWidget {{
    background-color: {BG};
    color: {TEXT};
    font-family: "Segoe UI", "DejaVu Sans", sans-serif;
    font-size: 13px;
}}
QFrame#Panel {{
    background-color: {PANEL};
    border: 1px solid {PANEL_BORDER};
    border-radius: 6px;
}}
QLabel#PanelTitle {{
    color: {TEXT_DIM};
    font-size: 11px;
    font-weight: 600;
    letter-spacing: 1px;
}}
QPushButton {{
    background-color: #21262d;
    border: 1px solid {PANEL_BORDER};
    border-radius: 6px;
    padding: 8px 10px;
    font-weight: 600;
}}
QPushButton:hover {{ background-color: #2d333b; }}
QPushButton:pressed {{ background-color: #1b1f24; }}
QPushButton:disabled {{ color: #484f58; border-color: #21262d; }}
QPushButton#Arm {{ background-color: #3fb950; color: #0d1117; border: none; }}
QPushButton#Arm:hover {{ background-color: #4fc960; }}
QPushButton#Disarm {{ background-color: {BAD}; color: #ffffff; border: none; }}
QPushButton#Disarm:hover {{ background-color: #ff6258; }}
QPushButton#Mode {{ font-size: 15px; padding: 12px 8px; }}
QPushButton#Connect {{ background-color: #238636; color: #ffffff; border: none; }}
QPushButton#Connect:hover {{ background-color: #2ea043; }}
QComboBox, QSpinBox, QLineEdit {{
    background-color: #0d1117;
    border: 1px solid {PANEL_BORDER};
    border-radius: 5px;
    padding: 5px 8px;
}}
QComboBox::drop-down {{ border: none; }}
QPlainTextEdit, QTextEdit {{
    background-color: #0a0d12;
    border: 1px solid {PANEL_BORDER};
    border-radius: 6px;
    font-family: "Consolas", "DejaVu Sans Mono", monospace;
    font-size: 12px;
}}
"""
