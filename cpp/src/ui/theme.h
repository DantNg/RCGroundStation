// Màu sắc tập trung + một stylesheet tối (một nơi duy nhất để đổi giao diện).
//
// Bảng màu lấy cảm hứng giao diện GitHub-dark của firmware cộng HUD kiểu Mission
// Planner (trời xanh / đất nâu / vạch trắng / ký hiệu máy bay hổ phách).
#pragma once

#include "domain/telemetry.h"

#include <QColor>
#include <QString>

namespace gcs::theme {

// ── giao diện chung ──────────────────────────────────────────────────────────
inline constexpr const char *BG = "#0a0e14";
inline constexpr const char *PANEL = "#141a22";
inline constexpr const char *PANEL_HI = "#1b232d";
inline constexpr const char *PANEL_BORDER = "#2a323d";
inline constexpr const char *TEXT = "#e6edf3";
inline constexpr const char *TEXT_DIM = "#8b97a7";
inline constexpr const char *ACCENT = "#ffb000";   // hổ phách
inline constexpr const char *ACCENT2 = "#4c9bff";  // xanh dương
inline constexpr const char *GOOD = "#3fb950";     // xanh lá
inline constexpr const char *BAD = "#ff5d52";      // đỏ
inline constexpr const char *WARN = "#e3a008";     // hổ phách cảnh báo

// ── HUD (bảng màu PFD/EFIS hiện đại) ─────────────────────────────────────────
inline QColor hudSkyTop()        { return QColor("#0b3a66"); }
inline QColor hudSkyHorizon()    { return QColor("#2f8fd6"); }
inline QColor hudGroundHorizon() { return QColor("#a06a30"); }
inline QColor hudGroundBottom()  { return QColor("#3c2913"); }
inline QColor hudLine()          { return QColor("#eef4f9"); }
inline QColor hudAccent()        { return QColor(ACCENT); }
inline QColor hudCyan()          { return QColor("#39e1ff"); }
inline QColor hudBoxBg()         { return QColor(8, 12, 18, 225); }
inline QColor hudText()          { return QColor("#ffffff"); }
inline QColor hudTextDim()       { return QColor("#9fb1bf"); }

// Màu theo mức độ nghiêm trọng cho nhật ký tin nhắn.
QString severityColor(domain::Severity sev);

// Toàn bộ QSS của ứng dụng.
QString stylesheet();

} // namespace gcs::theme
