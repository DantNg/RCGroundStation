// Màu sắc tập trung + một stylesheet tối (một nơi duy nhất để đổi giao diện).
//
// Bảng màu lấy cảm hứng giao diện GitHub-dark của firmware cộng HUD kiểu Mission
// Planner (trời xanh / đất nâu / vạch trắng / ký hiệu máy bay hổ phách).
#pragma once

#include "domain/telemetry.h"

#include <QColor>
#include <QIcon>
#include <QPixmap>
#include <QString>

namespace gcs::theme {

// ── giao diện chung ──────────────────────────────────────────────────────────
inline constexpr const char *BG = "#0a0e14";
inline constexpr const char *PANEL = "#141a22";
inline constexpr const char *PANEL_HI = "#1b232d";
inline constexpr const char *PANEL_BORDER = "#2a323d";
inline constexpr const char *TEXT = "#eef2f7";      // trắng ngà, tương phản cao
inline constexpr const char *TEXT_DIM = "#aab6c4";  // xám xanh sáng hơn (dễ đọc hơn)
inline constexpr const char *TEXT_MUTED = "#7c8899"; // chỉ dùng cho chú thích rất phụ
inline constexpr const char *ACCENT = "#ffb020";   // hổ phách
inline constexpr const char *ACCENT2 = "#5aa6ff";  // xanh dương
inline constexpr const char *GOOD = "#4cc463";     // xanh lá
inline constexpr const char *BAD = "#ff6459";      // đỏ
inline constexpr const char *WARN = "#f0a92a";     // hổ phách cảnh báo

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

// ── biểu tượng đơn sắc ───────────────────────────────────────────────────────
// Các PNG trong :/icons là đường nét đen trên nền trong suốt. Trên nền tối ta
// nhuộm lại theo màu mong muốn (giữ nguyên kênh alpha).
QPixmap tintedPixmap(const QString &resourcePath, const QColor &color);

// QIcon đơn sắc cho nút thanh hành động: sáng khi thường, tối khi được chọn
// (nền xanh), mờ khi vô hiệu — khớp với QSS của RailBtn.
QIcon railIcon(const QString &resourcePath);

// Toàn bộ QSS của ứng dụng.
QString stylesheet();

} // namespace gcs::theme
