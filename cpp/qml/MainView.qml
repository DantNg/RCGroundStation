// Vùng xem chính — chuyển đổi MAP (vệ tinh 2D) / CAM (FPV) / HUD (PFD).
// Hai view không hiển thị hiện dưới dạng ô PiP góc trên-phải, bấm để đổi.
import QtQuick
import GroundCtrl

Item {
    id: root
    clip: true

    property string mainView: "MAP"
    signal requestView(string view)

    readonly property var meta: ({ "MAP": "TAC MAP", "CAM": "FPV CAM", "HUD": "PFD HUD" })

    Rectangle { anchors.fill: parent; color: Theme.view }

    // ── ba view (chỉ view active hiện) ──────────────────────────────────────
    SatelliteMap { anchors.fill: parent; visible: root.mainView === "MAP" }
    CameraView   { anchors.fill: parent; visible: root.mainView === "CAM"; active: root.mainView === "CAM" }
    HudView      { anchors.fill: parent; visible: root.mainView === "HUD" }

    // ── ô PiP (2 view không active) ─────────────────────────────────────────
    Column {
        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: 8
        anchors.rightMargin: 8
        spacing: 8
        z: 22

        Repeater {
            model: ["MAP", "CAM", "HUD"].filter(v => v !== root.mainView)
            delegate: Rectangle {
                id: pip
                required property string modelData
                width: 134; height: 82; radius: 8; clip: true
                color: "#070a09"
                border.color: "#2a312b"

                // mini render theo loại view
                Loader {
                    anchors.fill: parent
                    sourceComponent: pip.modelData === "MAP" ? miniMap
                                     : (pip.modelData === "CAM" ? miniCam : miniHud)
                }

                // nhãn dưới
                Rectangle {
                    anchors.left: parent.left; anchors.right: parent.right; anchors.bottom: parent.bottom
                    height: 18
                    gradient: Gradient {
                        GradientStop { position: 0.0; color: "transparent" }
                        GradientStop { position: 1.0; color: "#d9000000" }
                    }
                    Text {
                        anchors.left: parent.left; anchors.leftMargin: 6; anchors.verticalCenter: parent.verticalCenter
                        text: root.meta[pip.modelData]; color: Theme.text
                        font.family: Theme.mono; font.pixelSize: 8; font.letterSpacing: 1
                    }
                    Text {
                        anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
                        text: "TAP ▸"; color: Theme.danger; font.family: Theme.mono; font.pixelSize: 7; font.letterSpacing: 0.5
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    cursorShape: Qt.PointingHandCursor
                    onClicked: root.requestView(pip.modelData)
                }
            }
        }
    }

    // ── overlay scanline mờ (đúng chất mockup) ──────────────────────────────
    Rectangle {
        anchors.fill: parent
        z: 45
        opacity: 0.5
        gradient: Gradient {
            GradientStop { position: 0.0; color: "#05ffffff" }
            GradientStop { position: 0.06; color: "transparent" }
        }
    }

    // ── mini render cho ô PiP ───────────────────────────────────────────────
    Component {
        id: miniMap
        Item {
            Rectangle { anchors.fill: parent; color: "#060a09" }
            // lưới nhỏ
            Canvas {
                anchors.fill: parent
                onPaint: {
                    const ctx = getContext("2d");
                    ctx.clearRect(0, 0, width, height);
                    ctx.strokeStyle = "rgba(120,140,135,0.14)"; ctx.lineWidth = 1;
                    for (let x = 0; x < width; x += 15) { ctx.beginPath(); ctx.moveTo(x, 0); ctx.lineTo(x, height); ctx.stroke(); }
                    for (let y = 0; y < height; y += 15) { ctx.beginPath(); ctx.moveTo(0, y); ctx.lineTo(width, y); ctx.stroke(); }
                }
            }
            Rectangle { x: parent.width * 0.52; y: parent.height * 0.44; width: 9; height: 9; radius: 4.5; color: Theme.danger }
            Rectangle { x: parent.width * 0.24; y: parent.height * 0.72; width: 8; height: 8; rotation: 45; color: Theme.accent }
        }
    }
    Component {
        id: miniCam
        Item {
            Rectangle {
                anchors.fill: parent
                gradient: Gradient {
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#0c100e" }
                    GradientStop { position: 0.5; color: "#0a0d0b" }
                    GradientStop { position: 1.0; color: "#0c100e" }
                }
            }
            Row {
                x: 6; y: 5; spacing: 3
                Rectangle { width: 5; height: 5; radius: 2.5; color: "#ff4d4d"; anchors.verticalCenter: parent.verticalCenter }
                Text { text: "REC"; color: "#ff6b6b"; font.family: Theme.mono; font.pixelSize: 7 }
            }
            Rectangle {
                anchors.centerIn: parent; width: 16; height: 16; color: "transparent"
                border.color: "#b3ff3b3b"; border.width: 1
            }
        }
    }
    Component {
        id: miniHud
        Item {
            clip: true
            // chân trời động theo roll/pitch (thu nhỏ) để ô PiP không đứng yên
            Rectangle {
                width: parent.width * 2; height: parent.height * 3
                x: (parent.width - width) / 2
                y: (parent.height - height) / 2 + telemetry.pitch * 1.2
                transformOrigin: Item.Center
                rotation: telemetry.roll
                gradient: Gradient {
                    GradientStop { position: 0.0; color: Theme.sky }
                    GradientStop { position: 0.495; color: Theme.sky }
                    GradientStop { position: 0.505; color: Theme.groundTop }
                    GradientStop { position: 1.0; color: Theme.ground }
                }
            }
            Rectangle { anchors.centerIn: parent; width: 22; height: 2; color: Theme.danger }
        }
    }
}
