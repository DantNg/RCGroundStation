// Thanh trạng thái trên (38px) — logo, ARM, MODE, T+, RF, GPS, RC, pin, ⚙, thoát.
// Bám mockup: nền gradient, viền dưới, chữ Chakra Petch + số JetBrains Mono.
import QtQuick
import QtQuick.Layouts
import GroundCtrl

Rectangle {
    id: root

    property bool settingsOpen: false
    signal settingsRequested()

    gradient: Gradient {
        GradientStop { position: 0.0; color: Theme.topbar1 }
        GradientStop { position: 1.0; color: Theme.topbar2 }
    }

    // viền dưới
    Rectangle { anchors.bottom: parent.bottom; width: parent.width; height: 1; color: Theme.strokeSoft }

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: 12
        anchors.rightMargin: 12
        spacing: 12

        // ── logo ────────────────────────────────────────────────────────────
        Row {
            spacing: 7
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                width: 9; height: 9; color: Theme.danger; rotation: 45
                anchors.verticalCenter: parent.verticalCenter
                layer.enabled: true
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                font.family: Theme.ui; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1.5
                textFormat: Text.StyledText
                text: "<span style='color:" + Theme.text + "'>GROUND</span><span style='color:"
                      + Theme.danger + "'>CTRL</span>"
            }
        }

        // ── nút ARM / DISARM ────────────────────────────────────────────────
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitHeight: 22
            implicitWidth: armTxt.implicitWidth + 20
            radius: 5
            color: telemetry.armed ? "#2a1113" : "#0e2018"
            border.color: telemetry.armed ? Theme.danger : Theme.accent
            opacity: telemetry.connected ? 1 : 0.5
            Text {
                id: armTxt
                anchors.centerIn: parent
                text: telemetry.armLabel
                color: telemetry.armed ? Theme.dangerSoft : Theme.accent
                font.family: Theme.mono; font.pixelSize: 11; font.bold: true; font.letterSpacing: 0.6
            }
            MouseArea {
                anchors.fill: parent
                enabled: telemetry.connected
                cursorShape: Qt.PointingHandCursor
                onClicked: telemetry.armed ? backend.disarm(false) : backend.arm(false)
            }
        }

        // ── MODE ────────────────────────────────────────────────────────────
        Row {
            spacing: 6
            Layout.alignment: Qt.AlignVCenter
            Text { anchors.verticalCenter: parent.verticalCenter
                   text: "MODE"; color: Theme.dim; font.family: Theme.ui; font.pixelSize: 9; font.letterSpacing: 1.5 }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: telemetry.connected ? telemetry.modeLabel : "—"
                color: telemetry.modeLabel === "GUIDED" ? Theme.danger
                       : (telemetry.modeLabel === "LAND" ? Theme.warn : Theme.accent)
                font.family: Theme.mono; font.pixelSize: 14; font.bold: true; font.letterSpacing: 0.6
            }
        }

        Item { Layout.fillWidth: true }

        // ── T+ (thời gian từ khi ARM) ───────────────────────────────────────
        MiniStat { label: "T+"; value: telemetry.armElapsed; valueColor: Theme.text }

        VSep {}

        // ── RF link ─────────────────────────────────────────────────────────
        MiniStat {
            label: "RF"; value: telemetry.linkPct + "%"
            valueColor: telemetry.linkPct < 40 && telemetry.connected ? Theme.warn : Theme.text
        }

        // ── GPS ─────────────────────────────────────────────────────────────
        MiniStat {
            label: "GPS"
            value: telemetry.satellites + "·" + telemetry.fixLabel
            valueColor: Theme.accent
        }

        // ── tay điều khiển RC (STM32) ───────────────────────────────────────
        MiniStat {
            label: "RC"
            value: rc.connected ? rc.fps + "Hz" : "OFF"
            valueColor: rc.connected ? Theme.accent : Theme.danger
        }

        VSep {}

        // ── pin ─────────────────────────────────────────────────────────────
        Row {
            spacing: 6
            Layout.alignment: Qt.AlignVCenter
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 26; height: 12; radius: 2; color: "transparent"
                border.color: "#4a534d"
                Rectangle {
                    x: 1; y: 1; height: parent.height - 2
                    width: Math.max(0, (parent.width - 2) * Math.max(0, telemetry.battPct) / 100)
                    radius: 1
                    color: telemetry.battPct > 50 ? Theme.accent
                           : (telemetry.battPct > 22 ? Theme.warn : Theme.danger)
                    visible: telemetry.battPct >= 0
                }
            }
            Text {
                anchors.verticalCenter: parent.verticalCenter
                text: telemetry.battPct < 0 ? "—" : telemetry.battPct + "%"
                color: Theme.text; font.family: Theme.mono; font.pixelSize: 11
            }
        }

        // ── cài đặt RC (công tắc / hiệu chỉnh / kênh) ───────────────────────
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 30; implicitHeight: 26; radius: 4
            color: root.settingsOpen ? "#2637e0a0" : "transparent"
            border.color: root.settingsOpen ? Theme.accent : Theme.stroke
            Text { anchors.centerIn: parent; text: "⚙"; color: root.settingsOpen ? Theme.accent : Theme.text
                   font.pixelSize: 15 }
            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: root.settingsRequested()
            }
        }

        // ── thoát (toàn màn hình không có nút X hệ thống) ───────────────────
        Rectangle {
            Layout.alignment: Qt.AlignVCenter
            implicitWidth: 30; implicitHeight: 26; radius: 4
            color: quitMa.containsMouse ? "#33ff3b3b" : "transparent"
            border.color: quitMa.containsMouse ? Theme.danger : Theme.stroke
            Text { anchors.centerIn: parent; text: "✕"; color: Theme.danger
                   font.family: Theme.mono; font.pixelSize: 12; font.bold: true }
            MouseArea {
                id: quitMa
                anchors.fill: parent
                hoverEnabled: true
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.quit()
            }
        }
    }

    // ── phần tử phụ ─────────────────────────────────────────────────────────
    component MiniStat: Row {
        property string label
        property string value
        property color valueColor: Theme.text
        spacing: 5
        Layout.alignment: Qt.AlignVCenter
        Text { anchors.verticalCenter: parent.verticalCenter
               text: label; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1 }
        Text { anchors.verticalCenter: parent.verticalCenter
               text: value; color: valueColor; font.family: Theme.mono; font.pixelSize: 11 }
    }

    component VSep: Rectangle {
        Layout.alignment: Qt.AlignVCenter
        width: 1; height: 16; color: Theme.stroke
    }
}
