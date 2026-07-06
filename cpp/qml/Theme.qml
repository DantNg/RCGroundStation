// Bảng màu + kiểu chữ dùng chung, theo mockup "GroundController Handheld".
// Singleton — mọi thành phần import GroundCtrl rồi dùng Theme.accent, Theme.mono…
//
// Font Chakra Petch (UI) + JetBrains Mono (số/nhãn) được nạp một lần trong Main
// qua FontLoader; ở đây chỉ giữ TÊN họ chữ (đã đăng ký toàn cục sau khi nạp).
pragma Singleton
import QtQuick

QtObject {
    // ── nền / khung ─────────────────────────────────────────────────────────
    readonly property color deepBg: "#040504"     // nền sau "thiết bị"
    readonly property color bg: "#0a0d0b"          // thân thiết bị
    readonly property color view: "#060a09"        // vùng xem chính (map/cam/hud)
    readonly property color panel: "#0e1210"       // nền nút/thẻ
    readonly property color panelSolid: "#0b0e0c"  // thanh dưới / rail
    readonly property color topbar1: "#0f1310"
    readonly property color topbar2: "#0b0e0c"

    // ── viền ────────────────────────────────────────────────────────────────
    readonly property color stroke: "#232a25"
    readonly property color strokeSoft: "#1a211d"
    readonly property color strokeBtn: "#20281f"

    // ── nhấn ────────────────────────────────────────────────────────────────
    readonly property color accent: "#37e0a0"      // lục — bình thường
    readonly property color danger: "#ff3b3b"      // đỏ — arm / GUIDED / cảnh báo
    readonly property color dangerSoft: "#ff6b6b"  // đỏ chữ nhạt hơn
    readonly property color warn: "#ffb020"        // hổ phách — cảnh báo / LAND

    // ── chữ ─────────────────────────────────────────────────────────────────
    readonly property color text: "#e9e7ee"
    readonly property color dim: "#6f6d7a"
    readonly property color sub: "#8a8894"

    // ── HUD (chân trời nhân tạo) ────────────────────────────────────────────
    readonly property color sky: "#1d5478"
    readonly property color skyTop: "#123a52"
    readonly property color ground: "#6e4a1f"
    readonly property color groundTop: "#c99a55"

    // ── họ chữ ──────────────────────────────────────────────────────────────
    readonly property string ui: "Chakra Petch"
    readonly property string mono: "JetBrains Mono"
}
