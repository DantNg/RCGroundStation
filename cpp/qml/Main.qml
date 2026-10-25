// Cửa sổ chính — bố cục "GroundController Handheld".
//
// Khung thiết bị cố định 800×480 (đúng tỉ lệ mockup, hợp màn cầm tay / Raspberry
// Pi), tự scale để vừa cửa sổ. Ba tầng: thanh trạng thái (trên) · hàng giữa
// [vùng xem chính | thanh chế độ 134px] · dải telemetry (dưới). Vùng xem chính
// chuyển đổi MAP (vệ tinh 2D) / CAM (FPV) / HUD (PFD) qua ô PiP.
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
    width: 800
    height: 480
    minimumWidth: 480
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

    // banner RC — chạm để mở trang cài đặt RC ở tab CẤU HÌNH
    component RcBanner: Rectangle {
        property string text
        property color tint: Theme.warn
        implicitWidth: bnTxt.implicitWidth + 24
        implicitHeight: 30
        radius: 6
        color: Qt.rgba(tint.r, tint.g, tint.b, 0.22)
        border.color: tint
        Text { id: bnTxt; anchors.centerIn: parent; text: "● " + parent.text; color: parent.tint
               font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
        MouseArea {
            anchors.fill: parent
            onClicked: { rcSettings.tab = "CFG"; device.settingsOpen = true }
        }
    }

    Item {
        id: viewport
        anchors.fill: parent

        // ── "thiết bị" cao 480 logic, scale-to-fit, bo góc ─────────────────
        // Rộng tối thiểu 800 và nở theo tỉ lệ màn hình để không bị viền đen:
        // màn DSI 1280×720 của Pi (16:9) → 853×480 logic, scale 1.5.
        Rectangle {
            id: device
            width: Math.max(800, Math.round(480 * viewport.width / Math.max(1, viewport.height)))
            height: 480
            anchors.centerIn: parent
            scale: Math.min(viewport.width / width, viewport.height / height)
            radius: win.visibility === Window.FullScreen ? 0 : 16
            color: Theme.bg
            border.color: Theme.stroke
            clip: true

            // view chính đang hiển thị: "MAP" | "CAM" | "HUD"
            property string mainView: "MAP"
            // trang cài đặt RC (⚙ trên thanh trạng thái)
            property bool settingsOpen: Qt.application.arguments.indexOf("--rc-setup") >= 0

            ColumnLayout {
                anchors.fill: parent
                spacing: 0

                StatusBar {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 38
                    settingsOpen: device.settingsOpen
                    onSettingsRequested: device.settingsOpen = !device.settingsOpen
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
                visible: !telemetry.connected && !device.settingsOpen
            }

            // ── cài đặt RC: phủ mọi thứ dưới thanh trạng thái ─────────────
            // ── banner RC (luôn hiện, guide §5/§6.2): cần lái giao cho Pi / chưa lưu ─
            // góc dưới-trái vùng xem (tránh banner cảnh báo trên-giữa và ô PiP)
            Column {
                anchors.left: parent.left
                anchors.leftMargin: 8
                anchors.bottom: parent.bottom
                anchors.bottomMargin: 80 + 8 // trên dải telemetry
                spacing: 6
                visible: !device.settingsOpen && rc.connected
                z: 50

                RcBanner {
                    visible: !rc.stickCrsf
                    text: "CẦN LÁI → PI · drone nhận giá trị giữ"
                    tint: Theme.danger
                }
                RcBanner {
                    visible: rc.dirty
                    text: "CẤU HÌNH RC CHƯA LƯU"
                    tint: Theme.warn
                }
            }

            RcSettings {
                id: rcSettings
                anchors.fill: parent
                anchors.topMargin: 38
                visible: device.settingsOpen
                onCloseRequested: device.settingsOpen = false
            }
        }
    }
}
