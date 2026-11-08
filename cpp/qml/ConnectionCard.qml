// Thẻ kết nối — nổi giữa màn khi chưa có link. Chọn UDP / TCP / Serial rồi Kết nối.
// Điền sẵn đường truyền đã lưu (mặc định SERIAL 115200); "KẾT NỐI LẠI" dùng lại nó.
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

    readonly property var saved: backend.savedLink
    property string mode: saved.type || "serial"

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
            SpinBox { id: udpPort; from: 1; to: 65535; value: root.saved.udpPort || 14550; editable: true; Layout.fillWidth: true }
        }

        // ── TCP ───────────────────────────────────────────────────────────
        ColumnLayout {
            visible: root.mode === "tcp"
            Layout.fillWidth: true; spacing: 6
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Host"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
                TextField { id: tcpHost; text: root.saved.tcpHost || "127.0.0.1"; Layout.fillWidth: true }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Cổng"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
                SpinBox { id: tcpPort; from: 1; to: 65535; value: root.saved.tcpPort || 5760; editable: true; Layout.fillWidth: true }
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
                    Component.onCompleted: currentIndex = Math.max(0, find(root.saved.serialPort))
                }
            }
            RowLayout {
                Layout.fillWidth: true; spacing: 8
                Label { text: "Baud"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 12 }
                ComboBox {
                    id: baud
                    Layout.fillWidth: true
                    model: [ 115200, 57600, 921600, 9600, 38400 ]
                    Component.onCompleted: currentIndex = Math.max(0, model.indexOf(root.saved.baud))
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

        // mở lại đúng đường truyền lần trước (vd sau khi cắm lại radio)
        Button {
            Layout.fillWidth: true
            visible: (root.saved.label || "") !== ""
            text: "↻ KẾT NỐI LẠI  " + (root.saved.label || "")
            onClicked: backend.reconnect()
        }
    }
}
