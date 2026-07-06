// Vùng xem HUD — PFD tổng hợp: chân trời nhân tạo (roll/pitch), ký hiệu máy bay
// cố định, badge AIRSPD/ALT, ô hướng trên, nhãn dưới. Bám mockup HUD.
import QtQuick
import GroundCtrl

Item {
    id: root
    clip: true

    Rectangle { anchors.fill: parent; color: "#05100e" }

    // ── chân trời (dịch theo pitch, xoay theo roll) ─────────────────────────
    Rectangle {
        id: horizon
        width: root.width * 2
        height: root.height * 3
        x: (root.width - width) / 2
        y: (root.height - height) / 2 + telemetry.pitch * 4
        transformOrigin: Item.Center
        rotation: telemetry.roll
        gradient: Gradient {
            GradientStop { position: 0.0; color: Theme.skyTop }
            GradientStop { position: 0.495; color: Theme.sky }
            GradientStop { position: 0.505; color: Theme.groundTop }
            GradientStop { position: 1.0; color: Theme.ground }
        }
    }

    // đường chân trời tham chiếu (cố định giữa)
    Rectangle { anchors.verticalCenter: parent.verticalCenter; width: parent.width; height: 1; color: "#4dffffff" }

    // ── ký hiệu máy bay cố định (giữa) ──────────────────────────────────────
    Row {
        anchors.centerIn: parent
        spacing: 7
        Rectangle { width: 28; height: 3; color: Theme.danger; anchors.verticalCenter: parent.verticalCenter }
        Rectangle { width: 8; height: 8; rotation: 45; color: "transparent"; border.color: Theme.danger; border.width: 2
                    anchors.verticalCenter: parent.verticalCenter }
        Rectangle { width: 28; height: 3; color: Theme.danger; anchors.verticalCenter: parent.verticalCenter }
    }

    // ── badge AIRSPD (trái) ─────────────────────────────────────────────────
    HudBadge {
        anchors.left: parent.left; anchors.leftMargin: 12; anchors.verticalCenter: parent.verticalCenter
        caption: "TỐC ĐỘ GIÓ"; value: telemetry.airspeed.toFixed(1); unit: "m/s"; valueColor: Theme.accent
    }
    // ── badge ALT REL (phải) ────────────────────────────────────────────────
    HudBadge {
        anchors.right: parent.right; anchors.rightMargin: 12; anchors.verticalCenter: parent.verticalCenter
        caption: "CAO ĐỘ"; value: telemetry.altRel.toFixed(1); unit: "m"; valueColor: Theme.text
    }

    // ── ô hướng (trên-giữa) ─────────────────────────────────────────────────
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.top: parent.top; anchors.topMargin: 8
        width: hdg.implicitWidth + 28; height: 22; radius: 4
        color: "#c7060c0a"; border.color: "#28ffffff"
        Text { id: hdg; anchors.centerIn: parent
               text: Math.round(telemetry.heading) + "° " + root.cardinal(telemetry.heading)
               color: Theme.text; font.family: Theme.mono; font.pixelSize: 12 }
    }

    // ── nhãn dưới ───────────────────────────────────────────────────────────
    Text {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        text: "MÀN HÌNH BAY CHÍNH · MÔ PHỎNG"; color: "#8a938d"
        font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1
    }

    function cardinal(h) {
        const dirs = ["N","NE","E","SE","S","SW","W","NW"];
        return dirs[Math.round((((h % 360) + 360) % 360) / 45) % 8];
    }

    component HudBadge: Rectangle {
        property string caption
        property string value
        property string unit
        property color valueColor: Theme.text
        width: 58; height: 60; radius: 6
        color: "#c7060c0a"; border.color: "#28ffffff"
        Column {
            anchors.centerIn: parent; spacing: 0
            Text { text: caption; color: "#8a938d"; font.family: Theme.mono; font.pixelSize: 8
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: value; color: valueColor; font.family: Theme.mono; font.pixelSize: 22; font.bold: true
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: unit; color: "#8a938d"; font.family: Theme.mono; font.pixelSize: 8
                   anchors.horizontalCenter: parent.horizontalCenter }
        }
    }
}
