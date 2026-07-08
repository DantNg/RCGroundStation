// Cửa sổ chính — bố cục "GroundController Handheld".
//
// Khung thiết bị nền 853×480 — tỉ lệ 16:9 để lấp ĐẦY màn 1280×720 (và các màn
// 16:9 khác) khi scale-to-fit, không còn viền đen hai bên như bản 800×480 cũ.
// Chiều cao vẫn 480 nên mọi thanh/cỡ chữ giữ nguyên tinh thần mockup; chỉ vùng
// xem chính rộng thêm. Ba tầng: thanh trạng thái (trên) · hàng giữa [vùng xem
// chính | thanh chế độ 134px] · dải telemetry (dưới). Vùng xem chính chuyển đổi
// MAP (vệ tinh 2D) / CAM (FPV) / HUD (PFD) qua ô PiP.
//
// Số liệu bind vào context property `telemetry`; lệnh gọi qua `backend`.
import QtQuick
import QtQuick.Controls
import QtQuick.Window
import QtQuick.Layouts
import GroundCtrl

ApplicationWindow {
    id: win
    visible: true
    width: 1280
    height: 720
    minimumWidth: 512
    minimumHeight: 288
    color: Theme.deepBg
    title: "Trạm Điều Khiển Mặt Đất — QML"

    // Toàn màn hình, không viền/tiêu đề (hợp màn cầm tay). Chạy `--windowed` để
    // mở dạng cửa sổ khi phát triển. Không còn nút X hệ thống → thoát bằng nút
    // "✕ THOÁT" trên thẻ kết nối hoặc góc phải thanh trạng thái.
    visibility: Qt.application.arguments.indexOf("--windowed") >= 0
                ? Window.Windowed : Window.FullScreen

    // ── nạp font mockup (Chakra Petch + JetBrains Mono, đã gộp latin + việt) ─
    // Dùng TTF (đã đổi từ woff2) vì FreeType của bản Qt này không đọc woff2.
    FontLoader { source: "fonts/JetBrainsMono-Regular.ttf" }
    FontLoader { source: "fonts/ChakraPetch-Medium.ttf" }
    FontLoader { source: "fonts/ChakraPetch-Bold.ttf" }

    Item {
        id: viewport
        anchors.fill: parent

        // ── "thiết bị" 853×480 (16:9), scale-to-fit, bo góc ─────────────────
        Rectangle {
            id: device
            width: 853.333          // 480 × 16/9 → lấp đầy màn 16:9 (vd 1280×720)
            height: 480
            anchors.centerIn: parent
            scale: Math.min(viewport.width / width, viewport.height / height)
            radius: 16
            color: Theme.bg
            border.color: Theme.stroke
            clip: true

            // view chính đang hiển thị: "MAP" | "CAM" | "HUD"
            property string mainView: "MAP"

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                StatusBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 0

                    MainView {
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        mainView: device.mainView
                        onRequestView: (v) => device.mainView = v
                    }

                    ModeRail {
                        Layout.preferredWidth: 134
                        Layout.fillHeight: true
                    }
                }

                TelemetryStrip {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 80
                }
            }

            // ── lớp phủ cần lái ảo (trên vùng xem chính, dưới rail 134) ────
            // Phủ đúng vùng xem: dưới thanh trạng thái (38), trên dải telemetry
            // (80), trừ thanh chế độ bên phải (134). Tự ẩn khi joystick tắt.
            JoystickOverlay {
                anchors.fill: parent
                anchors.topMargin: 38
                anchors.bottomMargin: 80
                anchors.rightMargin: 134
                z: 40
            }

            // ── banner cảnh báo (trên-giữa, trên vùng xem) ─────────────────
            WarningBanners {
                width: Math.min(560, device.width - 200)
                anchors.top: parent.top
                anchors.topMargin: 46
                anchors.horizontalCenter: parent.horizontalCenter
                anchors.horizontalCenterOffset: -67 // canh giữa vùng xem (trừ rail 134)
            }

            // ── thẻ kết nối (giữa, khi chưa kết nối) ───────────────────────
            ConnectionCard {
                anchors.centerIn: parent
                visible: !telemetry.connected
            }
        }
    }
}
