"""Centralised colours and a dark stylesheet (one place to restyle the app).

Palette echoes the firmware's GitHub-dark UI plus a Mission-Planner-style HUD
(blue sky / brown ground / white lines / amber aircraft symbol). The QSS aims
for a modern, glanceable fly-view: soft elevated cards, rounded controls, a
clear accent, and thin scrollbars — readable from a wide desktop down to a small
high-DPI panel.
"""
from __future__ import annotations

from PySide6.QtGui import QColor

from ..domain.telemetry import Severity

# ── general UI ──────────────────────────────────────────────────────────────
BG = "#0a0e14"            # app backdrop (a touch deeper than the cards)
PANEL = "#141a22"         # card surface
PANEL_HI = "#1b232d"      # raised / hover surface
PANEL_BORDER = "#2a323d"  # hairline border
TEXT = "#e6edf3"
TEXT_DIM = "#8b97a7"
ACCENT = "#ffb000"        # amber — aircraft symbol, active highlights
ACCENT2 = "#4c9bff"       # blue — interactive accents (focus, links)
GOOD = "#3fb950"          # green — connected / disarmed-safe
BAD = "#ff5d52"           # red — error / armed / disconnected
WARN = "#e3a008"          # amber — warnings

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
* {{
    outline: none;
}}
QWidget {{
    background-color: {BG};
    color: {TEXT};
    font-family: "Segoe UI Variable", "Segoe UI", "Inter", "DejaVu Sans", sans-serif;
    font-size: 13px;
}}
QToolTip {{
    background-color: {PANEL_HI};
    color: {TEXT};
    border: 1px solid {PANEL_BORDER};
    border-radius: 6px;
    padding: 5px 8px;
}}

/* ── cards ──────────────────────────────────────────────────────────────── */
QFrame#Panel {{
    background-color: rgba(20, 26, 34, 0.94);
    border: 1px solid {PANEL_BORDER};
    border-radius: 12px;
}}
QLabel#PanelTitle {{
    color: {TEXT_DIM};
    font-size: 11px;
    font-weight: 700;
    letter-spacing: 1.5px;
}}

/* ── buttons ────────────────────────────────────────────────────────────── */
QPushButton {{
    background-color: {PANEL_HI};
    border: 1px solid {PANEL_BORDER};
    border-radius: 8px;
    padding: 8px 12px;
    font-weight: 600;
}}
QPushButton:hover {{ background-color: #232c37; border-color: #3a4654; }}
QPushButton:pressed {{ background-color: #161d25; }}
QPushButton:disabled {{ color: #4b5360; background-color: #131820; border-color: #20272f; }}
QPushButton:checked {{
    background-color: #243140;
    border-color: {ACCENT2};
    color: {TEXT};
}}

QPushButton#Arm {{ background-color: {GOOD}; color: #062a10; border: none; }}
QPushButton#Arm:hover {{ background-color: #4fd463; }}
QPushButton#Disarm {{ background-color: {BAD}; color: #2a0707; border: none; }}
QPushButton#Disarm:hover {{ background-color: #ff7068; }}
QPushButton#Mode {{ font-size: 15px; padding: 12px 8px; }}
QPushButton#Connect {{ background-color: #238636; color: #ffffff; border: none; padding: 8px 18px; }}
QPushButton#Connect:hover {{ background-color: #2ea043; }}

/* small square icon button (refresh, zoom, swap…) */
QPushButton#IconButton {{
    padding: 6px;
    min-width: 30px;
    border-radius: 8px;
    font-size: 15px;
    font-weight: 700;
}}
/* quiet "ghost" button used on chips/overlays */
QPushButton#Ghost {{
    background-color: rgba(255, 255, 255, 0.06);
    border: 1px solid rgba(255, 255, 255, 0.10);
    border-radius: 8px;
    padding: 5px 12px;
}}
QPushButton#Ghost:hover {{ background-color: rgba(255, 255, 255, 0.12); }}
QPushButton#ChipDisconnect {{
    background-color: rgba(255, 93, 82, 0.16);
    border: 1px solid rgba(255, 93, 82, 0.45);
    color: #ff8a82;
    border-radius: 8px;
    padding: 5px 12px;
}}
QPushButton#ChipDisconnect:hover {{ background-color: rgba(255, 93, 82, 0.28); }}

/* ── inputs ─────────────────────────────────────────────────────────────── */
QComboBox, QSpinBox, QLineEdit {{
    background-color: #0c1118;
    border: 1px solid {PANEL_BORDER};
    border-radius: 8px;
    padding: 6px 9px;
    selection-background-color: {ACCENT2};
}}
QComboBox:hover, QSpinBox:hover, QLineEdit:hover {{ border-color: #3a4654; }}
QComboBox:focus, QSpinBox:focus, QLineEdit:focus {{ border-color: {ACCENT2}; }}
QComboBox::drop-down {{ border: none; width: 20px; }}
QComboBox::down-arrow {{
    image: none;
    border-left: 4px solid transparent;
    border-right: 4px solid transparent;
    border-top: 5px solid {TEXT_DIM};
    margin-right: 8px;
}}
QComboBox QAbstractItemView {{
    background-color: {PANEL};
    border: 1px solid {PANEL_BORDER};
    border-radius: 8px;
    selection-background-color: {PANEL_HI};
    outline: none;
    padding: 4px;
}}

/* ── check box ──────────────────────────────────────────────────────────── */
QCheckBox {{ spacing: 7px; color: {TEXT_DIM}; }}
QCheckBox::indicator {{
    width: 16px; height: 16px;
    border: 1px solid {PANEL_BORDER};
    border-radius: 5px;
    background-color: #0c1118;
}}
QCheckBox::indicator:hover {{ border-color: {ACCENT2}; }}
QCheckBox::indicator:checked {{
    background-color: {ACCENT2};
    border-color: {ACCENT2};
}}

/* ── text panels ────────────────────────────────────────────────────────── */
QPlainTextEdit, QTextEdit {{
    background-color: #080b10;
    border: 1px solid {PANEL_BORDER};
    border-radius: 8px;
    font-family: "Cascadia Mono", "Consolas", "DejaVu Sans Mono", monospace;
    font-size: 12px;
}}

/* ── thin scrollbars ────────────────────────────────────────────────────── */
QScrollBar:vertical {{ background: transparent; width: 10px; margin: 2px; }}
QScrollBar::handle:vertical {{
    background: #313b47; border-radius: 5px; min-height: 28px;
}}
QScrollBar::handle:vertical:hover {{ background: #3e4a59; }}
QScrollBar::add-line:vertical, QScrollBar::sub-line:vertical {{ height: 0; }}
QScrollBar::add-page:vertical, QScrollBar::sub-page:vertical {{ background: transparent; }}
QScrollBar:horizontal {{ background: transparent; height: 10px; margin: 2px; }}
QScrollBar::handle:horizontal {{
    background: #313b47; border-radius: 5px; min-width: 28px;
}}
QScrollBar::handle:horizontal:hover {{ background: #3e4a59; }}
QScrollBar::add-line:horizontal, QScrollBar::sub-line:horizontal {{ width: 0; }}
QScrollBar::add-page:horizontal, QScrollBar::sub-page:horizontal {{ background: transparent; }}

/* ── connection status chip (shown once a link is up) ───────────────────── */
QFrame#ConnChip {{
    background-color: rgba(20, 26, 34, 0.92);
    border: 1px solid {PANEL_BORDER};
    border-radius: 14px;
}}
QLabel#ConnLabel {{ font-weight: 600; color: {TEXT}; }}

/* ── combined top pill (connection status + active view's controls) ─────── */
QFrame#TopBar {{
    background-color: rgba(20, 26, 34, 0.92);
    border: 1px solid {PANEL_BORDER};
    border-radius: 12px;
}}
QFrame#TopBar QLabel {{ color: {TEXT_DIM}; }}
QFrame#TopBar QLabel#ConnLabel {{ color: {TEXT}; font-weight: 600; }}
QFrame#TopBar QPushButton {{ padding: 5px 10px; }}
QFrame#TopBar QPushButton#IconButton {{ min-width: 26px; padding: 4px; font-size: 14px; }}
QFrame#TopBar QComboBox {{ padding: 4px 8px; }}

/* ── camera placeholder (font size is set dynamically to match the tile) ── */
QLabel#CamPlaceholder {{ color: {TEXT_DIM}; }}
"""
