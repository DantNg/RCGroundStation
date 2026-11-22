// Thẻ kết nối — nổi giữa màn khi chưa có link.
// Trên cùng: đường truyền lần trước + nút KẾT NỐI LẠI (một chạm). Dưới: chọn
// UDP / TCP / SERIAL bằng chip lớn cho màn cảm ứng (không dùng ComboBox/SpinBox
// mặc định). Cổng serial của tay cầm RC (rc.portName) bị khoá để không chọn nhầm.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GroundCtrl

Rectangle {
    id: root
    implicitWidth: 470
    implicitHeight: col.implicitHeight + 28
    radius: 14
    color: "#f50a0d0b"
    border.color: Theme.stroke

    readonly property var saved: backend.savedLink
    property string mode: saved.type || "serial"
    property var ports: backend.serialPorts()
    property string port: ""
    property int baud: saved.baud || 115200
    property int udpPort: saved.udpPort || 14550
    property int tcpPort: saved.tcpPort || 5760

    readonly property var bauds: [57600, 115200, 230400, 460800, 921600]
    readonly property string rcPortName: rc.portName.replace("/dev/", "")
    readonly property bool canConnect: mode !== "serial" || (port !== "" && port !== rcPortName)

    function refreshPorts() {
        ports = backend.serialPorts()
        if (ports.indexOf(port) < 0)
            port = ports.indexOf(saved.serialPort) >= 0 ? saved.serialPort
                 : (ports.find(p => p !== rcPortName) || "")
    }
    function connectNow() {
        if (mode === "udp") backend.connectUdp(udpPort)
        else if (mode === "tcp") backend.connectTcp(tcpHost.text, tcpPort)
        else backend.connectSerial(port, baud)
    }
    Component.onCompleted: refreshPorts()
    onVisibleChanged: if (visible) refreshPorts()

    // chặn chạm xuyên xuống bản đồ
    MouseArea { anchors.fill: parent }

    ColumnLayout {
        id: col
        anchors.fill: parent
        anchors.margins: 14
        spacing: 8

        // ── tiêu đề ───────────────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 0
            Text { text: "GROUND"; color: Theme.text; font.family: Theme.mono; font.pixelSize: 18; font.bold: true }
            Text { text: "CTRL"; color: Theme.danger; font.family: Theme.mono; font.pixelSize: 18; font.bold: true }
            Item { Layout.fillWidth: true }
            Rectangle {
                implicitWidth: stTxt.implicitWidth + 22; implicitHeight: 22; radius: 11
                color: "#1aff3b3b"; border.color: "#55ff3b3b"
                Row {
                    anchors.centerIn: parent
                    spacing: 6
                    Rectangle {
                        width: 7; height: 7; radius: 3.5; color: Theme.danger
                        anchors.verticalCenter: parent.verticalCenter
                        SequentialAnimation on opacity {
                            loops: Animation.Infinite
                            NumberAnimation { from: 1; to: 0.25; duration: 700 }
                            NumberAnimation { from: 0.25; to: 1; duration: 700 }
                        }
                    }
                    Text { id: stTxt; text: "CHƯA KẾT NỐI"; color: Theme.dangerSoft
                           font.family: Theme.mono; font.pixelSize: 10; font.bold: true }
                }
            }
        }

        // ── lần trước: một chạm ───────────────────────────────────────────
        Rectangle {
            visible: (root.saved.label || "") !== ""
            Layout.fillWidth: true
            Layout.preferredHeight: 50
            radius: 10
            color: "#0f1f18"
            border.color: "#2a5c47"
            RowLayout {
                anchors.fill: parent
                anchors.leftMargin: 14; anchors.rightMargin: 8
                spacing: 10
                ColumnLayout {
                    spacing: 1
                    Layout.fillWidth: true
                    Text { text: "LẦN TRƯỚC · " + (root.saved.type || "").toUpperCase(); color: Theme.dim
                           font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1 }
                    Text { text: root.saved.label || ""; color: Theme.text
                           font.family: Theme.mono; font.pixelSize: 15; font.bold: true
                           Layout.fillWidth: true; elide: Text.ElideRight }
                }
                CardBtn {
                    text: "↻  KẾT NỐI LẠI"
                    outline: true
                    Layout.preferredHeight: 40
                    onClicked: backend.reconnect()
                }
            }
        }

        // ── kiểu đường truyền ─────────────────────────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6
            Repeater {
                model: [ { k: "serial", t: "SERIAL", d: "Radio telemetry" },
                         { k: "udp", t: "UDP", d: "Wi-Fi / mạng" },
                         { k: "tcp", t: "TCP", d: "SITL / máy chủ" } ]
                delegate: Rectangle {
                    required property var modelData
                    readonly property bool sel: root.mode === modelData.k
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: 8
                    color: sel ? "#143327" : Theme.panel
                    border.color: sel ? Theme.accent : Theme.strokeBtn
                    border.width: sel ? 1.5 : 1
                    Column {
                        anchors.centerIn: parent
                        spacing: 1
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.t
                               color: parent.parent.sel ? Theme.accent : Theme.text
                               font.family: Theme.mono; font.pixelSize: 13; font.bold: true }
                        Text { anchors.horizontalCenter: parent.horizontalCenter; text: modelData.d
                               color: Theme.dim; font.family: Theme.ui; font.pixelSize: 9 }
                    }
                    MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                                onClicked: root.mode = modelData.k }
                }
            }
        }

        // ── SERIAL: cổng + baud ───────────────────────────────────────────
        ColumnLayout {
            visible: root.mode === "serial"
            Layout.fillWidth: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                FieldLabel { text: "CỔNG" }
                Item { Layout.fillWidth: true }
                Text {
                    text: "↻ tìm lại"
                    color: Theme.sub; font.family: Theme.mono; font.pixelSize: 10
                    MouseArea { anchors.fill: parent; anchors.margins: -8; onClicked: root.refreshPorts() }
                }
            }
            Flickable {
                Layout.fillWidth: true
                Layout.preferredHeight: 34
                contentWidth: portRow.width
                clip: true
                boundsBehavior: Flickable.StopAtBounds
                Row {
                    id: portRow
                    spacing: 6
                    Repeater {
                        model: root.ports
                        delegate: Chip {
                            required property string modelData
                            readonly property bool isRc: modelData === root.rcPortName
                            text: modelData + (isRc ? " · RC" : "")
                            selected: root.port === modelData
                            enabled: !isRc
                            onClicked: root.port = modelData
                        }
                    }
                    Text {
                        visible: root.ports.length === 0
                        height: 36; verticalAlignment: Text.AlignVCenter
                        text: "Không thấy cổng serial nào"
                        color: Theme.dangerSoft; font.family: Theme.ui; font.pixelSize: 12
                    }
                }
            }

            FieldLabel { text: "BAUD" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: root.bauds.indexOf(root.baud) >= 0 ? root.bauds : root.bauds.concat([root.baud])
                    delegate: Chip {
                        required property int modelData
                        Layout.fillWidth: true
                        text: modelData
                        selected: root.baud === modelData
                        onClicked: root.baud = modelData
                    }
                }
            }
        }

        // ── UDP ───────────────────────────────────────────────────────────
        ColumnLayout {
            visible: root.mode === "udp"
            Layout.fillWidth: true
            spacing: 6
            FieldLabel { text: "CỔNG UDP NGHE" }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Stepper { value: root.udpPort; onValueModified: v => root.udpPort = v; Layout.fillWidth: true }
                Chip { text: "14550"; selected: root.udpPort === 14550; onClicked: root.udpPort = 14550 }
                Chip { text: "14551"; selected: root.udpPort === 14551; onClicked: root.udpPort = 14551 }
            }
        }

        // ── TCP ───────────────────────────────────────────────────────────
        ColumnLayout {
            visible: root.mode === "tcp"
            Layout.fillWidth: true
            spacing: 6
            FieldLabel { text: "MÁY CHỦ" }
            TextField {
                id: tcpHost
                Layout.fillWidth: true
                Layout.preferredHeight: 36
                text: root.saved.tcpHost || "127.0.0.1"
                color: Theme.text
                font.family: Theme.mono; font.pixelSize: 14
                leftPadding: 12
                background: Rectangle { radius: 8; color: Theme.bg
                                        border.color: tcpHost.activeFocus ? Theme.accent : Theme.strokeBtn }
            }
            FieldLabel { text: "CỔNG TCP" }
            Stepper { value: root.tcpPort; onValueModified: v => root.tcpPort = v; Layout.fillWidth: true }
        }

        // ── kết nối ───────────────────────────────────────────────────────
        CardBtn {
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            primary: true
            enabled: root.canConnect
            text: root.mode === "serial"
                  ? "KẾT NỐI  " + (root.port !== "" ? root.port + " @" + root.baud : "")
                  : root.mode === "udp" ? "NGHE UDP :" + root.udpPort
                  : "KẾT NỐI " + tcpHost.text + ":" + root.tcpPort
            onClicked: root.connectNow()
        }
    }

    // ── thành phần nhỏ ──────────────────────────────────────────────────────
    component FieldLabel: Text {
        color: Theme.dim; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1.2
    }

    component Chip: Rectangle {
        id: chip
        property alias text: chipTxt.text
        property bool selected: false
        signal clicked()
        implicitWidth: chipTxt.implicitWidth + 24
        implicitHeight: 34
        radius: 8
        opacity: enabled ? 1 : 0.4
        color: selected ? "#143327" : (chipMa.pressed ? "#18201b" : Theme.panel)
        border.color: selected ? Theme.accent : Theme.strokeBtn
        border.width: selected ? 1.5 : 1
        Text { id: chipTxt; anchors.centerIn: parent
               color: chip.selected ? Theme.accent : Theme.text
               font.family: Theme.mono; font.pixelSize: 12; font.bold: chip.selected }
        MouseArea { id: chipMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: chip.clicked() }
    }

    component Stepper: Rectangle {
        id: st
        property int value
        property int from: 1
        property int to: 65535
        signal valueModified(int v)
        function bump(d) { valueModified(Math.max(from, Math.min(to, value + d))) }
        implicitHeight: 36
        radius: 8
        color: Theme.bg
        border.color: Theme.strokeBtn
        RowLayout {
            anchors.fill: parent
            spacing: 0
            StepKey { text: "−"; onClicked: st.bump(-1) }
            Text { Layout.fillWidth: true; horizontalAlignment: Text.AlignHCenter
                   text: st.value; color: Theme.text; font.family: Theme.mono; font.pixelSize: 15; font.bold: true }
            StepKey { text: "+"; onClicked: st.bump(1) }
        }
    }

    component StepKey: Rectangle {
        id: sk
        property string text
        signal clicked()
        Layout.preferredWidth: 44
        Layout.fillHeight: true
        radius: 8
        color: skMa.pressed ? "#1d2520" : "transparent"
        Text { anchors.centerIn: parent; text: sk.text; color: Theme.text; font.pixelSize: 18 }
        MouseArea { id: skMa; anchors.fill: parent; onClicked: sk.clicked() }
    }

    component CardBtn: Rectangle {
        id: btn
        property string text
        property bool primary: false
        property bool outline: false   // viền + chữ xanh (hành động phụ)
        signal clicked()
        implicitWidth: btnTxt.implicitWidth + 28
        implicitHeight: 40
        radius: 9
        opacity: enabled ? 1 : 0.4
        color: primary ? (btnMa.pressed ? "#2fbf88" : Theme.accent)
               : (btnMa.pressed ? "#1d2520" : (outline ? "#0f2a1f" : Theme.panel))
        border.color: primary || outline ? Theme.accent : Theme.strokeBtn
        Text { id: btnTxt; anchors.centerIn: parent; text: btn.text
               color: btn.primary ? "#04140d" : (btn.outline ? Theme.accent : Theme.text)
               font.family: Theme.mono; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.5 }
        MouseArea { id: btnMa; anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                    onClicked: btn.clicked() }
    }
}
