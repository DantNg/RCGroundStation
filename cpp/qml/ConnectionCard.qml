// Thẻ kết nối — nổi giữa màn khi chưa có link. Chọn UDP / TCP / Serial rồi Kết nối.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GroundCtrl

Rectangle {
    id: root
    implicitWidth: 380
    implicitHeight: col.implicitHeight + 40
    radius: 12
    color: "#f20a0d0b"
    border.color: Theme.stroke

    property string mode: "udp"

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 20
        spacing: 14

        RowLayout {
            spacing: 0
            Text { text: "GROUND"; color: Theme.text; font.family: Theme.mono; font.pixelSize: 20; font.bold: true }
            Text { text: "CTRL"; color: Theme.danger; font.family: Theme.mono; font.pixelSize: 20; font.bold: true }
            Item { Layout.fillWidth: true }
            Text { text: "CHƯA KẾT NỐI"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 11 }
        }

        // segmented UDP / TCP / SERIAL
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [ { k: "udp", t: "UDP" }, { k: "tcp", t: "TCP" }, { k: "serial", t: "SERIAL" } ]
                delegate: Rectangle {
                    required property var modelData
                    Layout.fillWidth: true
                    height: 34; radius: 6
                    readonly property bool sel: root.mode === modelData.k
                    color: sel ? Theme.accent : Theme.panelSolid
                    border.color: sel ? Theme.accent : Theme.stroke
                    Text { anchors.centerIn: parent; text: modelData.t
                           color: parent.sel ? "#04140d" : Theme.text
                           font.family: Theme.mono; font.pixelSize: 12; font.bold: true }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: root.mode = modelData.k }
                }
            }
        }

        // ── UDP ───────────────────────────────────────────────────────────
        RowLayout {
            visible: root.mode === "udp"
            Layout.fillWidth: true; spacing: 8
            Label { text: "Cổng UDP"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
            SpinBox { id: udpPort; from: 1; to: 65535; value: 14550; editable: true; Layout.fillWidth: true }
        }

        // ── TCP ───────────────────────────────────────────────────────────
        ColumnLayout {
            visible: root.mode === "tcp"
            Layout.fillWidth: true; spacing: 6
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Host"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
                TextField { id: tcpHost; text: "127.0.0.1"; Layout.fillWidth: true }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Cổng"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
                SpinBox { id: tcpPort; from: 1; to: 65535; value: 5760; editable: true; Layout.fillWidth: true }
            }
        }

        // ── SERIAL ────────────────────────────────────────────────────────
        ColumnLayout {
            visible: root.mode === "serial"
            Layout.fillWidth: true; spacing: 6
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Cổng"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
                ComboBox {
                    id: serialPort
                    Layout.fillWidth: true
                    model: backend.serialPorts()
                    Component.onCompleted: if (count > 0) currentIndex = 0
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Baud"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
                ComboBox {
                    id: baud
                    Layout.fillWidth: true
                    model: [ 57600, 115200, 921600, 9600, 38400 ]
                }
            }
        }

        // ── chế độ cầu nối: phát MAVLink ra Wi-Fi cho máy tính (MP/QGC) đọc ──
        Rectangle {
            Layout.fillWidth: true
            radius: 8
            color: Theme.panelSolid
            border.color: bridgeSwitch.checked ? Theme.accent : Theme.stroke
            implicitHeight: bridgeCol.implicitHeight + 20

            ColumnLayout {
                id: bridgeCol
                anchors.fill: parent
                anchors.margins: 10
                spacing: 6

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    ColumnLayout {
                        spacing: 0
                        Text { text: "CHẾ ĐỘ CẦU NỐI"; color: Theme.text
                               font.family: Theme.mono; font.pixelSize: 12; font.bold: true }
                        Text { text: "Phát MAVLink ra Wi-Fi cho máy tính"; color: Theme.dim
                               font.family: Theme.mono; font.pixelSize: 10 }
                    }
                    Item { Layout.fillWidth: true }
                    Switch {
                        id: bridgeSwitch
                        checked: backend.bridgeEnabled
                        onToggled: backend.setBridgeMode(checked, bridgePort.value)
                    }
                }

                RowLayout {
                    visible: bridgeSwitch.checked
                    Layout.fillWidth: true; spacing: 8
                    Label { text: "Cổng broadcast"; color: Theme.dim
                            font.family: Theme.mono; font.pixelSize: 12 }
                    SpinBox {
                        id: bridgePort
                        from: 1; to: 65535; value: backend.bridgePort; editable: true
                        Layout.fillWidth: true
                        onValueModified: if (bridgeSwitch.checked) backend.setBridgeMode(true, value)
                    }
                }
            }
        }

        Button {
            Layout.fillWidth: true
            text: "KẾT NỐI"
            enabled: root.mode !== "serial" || serialPort.count > 0
            onClicked: {
                if (root.mode === "udp") backend.connectUdp(udpPort.value);
                else if (root.mode === "tcp") backend.connectTcp(tcpHost.text, tcpPort.value);
                else backend.connectSerial(serialPort.currentText, parseInt(baud.currentText));
            }
        }

        // ── thoát ứng dụng (toàn màn hình nên không có nút X hệ thống) ───────
        Rectangle {
            Layout.fillWidth: true
            Layout.preferredHeight: 34
            radius: 6
            color: exitMa.pressed ? "#33ff3b3b" : "transparent"
            border.color: Theme.danger
            Text {
                anchors.centerIn: parent; text: "✕ THOÁT"
                color: Theme.danger; font.family: Theme.mono; font.pixelSize: 12; font.bold: true; font.letterSpacing: 0.6
            }
            MouseArea {
                id: exitMa
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                onClicked: Qt.quit()
            }
        }
    }
}
