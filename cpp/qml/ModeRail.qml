// Thanh chế độ bay (phải, 134px) — tiêu đề FLIGHT MODE + các nút chế độ nhanh
// từ backend.quickModes. Nút đang khớp telemetry.modeLabel sáng đỏ như mockup.
import QtQuick
import QtQuick.Layouts
import GroundCtrl

Rectangle {
    id: root
    color: Theme.panelSolid

    // độ cao cất cánh mặc định (m) — hợp vùng bay hẹp
    property real takeoffAlt: 5

    // viền trái
    Rectangle { anchors.left: parent.left; width: 1; height: parent.height; color: Theme.strokeSoft }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 8
        anchors.leftMargin: 9
        spacing: 6

        Text {
            Layout.fillWidth: true
            text: "FLIGHT MODE"; color: Theme.dim
            font.family: Theme.ui; font.pixelSize: 9; font.bold: true; font.letterSpacing: 1.6
        }

        Repeater {
            model: backend.quickModes
            delegate: Rectangle {
                required property var modelData
                readonly property bool active: telemetry.connected && telemetry.modeLabel === modelData.mode
                Layout.fillWidth: true
                Layout.fillHeight: true
                radius: 6
                border.color: active ? Theme.danger : Theme.strokeBtn
                gradient: active ? activeGrad : null
                color: active ? "transparent" : Theme.panel
                opacity: telemetry.connected ? 1 : 0.45

                Gradient {
                    id: activeGrad
                    orientation: Gradient.Horizontal
                    GradientStop { position: 0.0; color: "#42ff3b3b" }
                    GradientStop { position: 1.0; color: "#0aff3b3b" }
                }

                Column {
                    anchors.left: parent.left; anchors.leftMargin: 10
                    anchors.verticalCenter: parent.verticalCenter
                    spacing: 1
                    Text {
                        text: modelData.label
                        color: parent.parent.active ? "#ffe9e9" : "#b9b7c2"
                        font.family: Theme.mono; font.pixelSize: 12; font.bold: true; font.letterSpacing: 0.4
                    }
                    Text {
                        width: root.width - 30
                        text: modelData.desc
                        color: parent.parent.active ? "#e9c9c9" : Theme.dim
                        font.family: Theme.ui; font.pixelSize: 8
                        elide: Text.ElideRight; maximumLineCount: 1
                        opacity: 0.85
                    }
                }

                MouseArea {
                    anchors.fill: parent
                    enabled: telemetry.connected
                    cursorShape: Qt.PointingHandCursor
                    onClicked: backend.setMode(modelData.mode)
                }
            }
        }

        // ── hành động cất/hạ cánh ───────────────────────────────────────────
        Rectangle { Layout.fillWidth: true; Layout.preferredHeight: 1; color: Theme.strokeSoft }

        ActionBtn {
            glyph: "▲"; label: "TAKEOFF"; sub: "GUIDED · " + root.takeoffAlt + "m"; tint: Theme.accent
            onActivated: backend.takeoff(root.takeoffAlt)
        }
        ActionBtn {
            glyph: "▼"; label: "LAND"; sub: "hạ cánh & disarm"; tint: Theme.warn
            onActivated: backend.setMode("LAND")
        }
    }

    // nút hành động lớn (TAKEOFF / LAND) — tô theo tint, chỉ bấm khi đã kết nối
    component ActionBtn: Rectangle {
        property string glyph
        property string label
        property string sub
        property color tint: Theme.accent
        signal activated()

        Layout.fillWidth: true
        Layout.preferredHeight: 40
        radius: 6
        color: Qt.rgba(tint.r, tint.g, tint.b, 0.14)
        border.color: tint
        opacity: telemetry.connected ? 1 : 0.4

        Row {
            anchors.left: parent.left; anchors.leftMargin: 9
            anchors.verticalCenter: parent.verticalCenter
            spacing: 7
            Text { text: glyph; color: tint; font.pixelSize: 15; font.bold: true
                   anchors.verticalCenter: parent.verticalCenter }
            Column {
                anchors.verticalCenter: parent.verticalCenter
                Text { text: label; color: tint; font.family: Theme.mono; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.5 }
                Text { text: sub; color: Theme.dim; font.family: Theme.ui; font.pixelSize: 8 }
            }
        }
        MouseArea {
            anchors.fill: parent
            enabled: telemetry.connected
            cursorShape: Qt.PointingHandCursor
            onClicked: parent.activated()
        }
    }
}
