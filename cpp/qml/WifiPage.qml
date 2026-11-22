// Tab WI-FI trong trang cài đặt: mạng đang dùng (SSID / IP / tín hiệu) + danh
// sách mạng quét được. Chạm mạng → nhập mật khẩu bằng bàn phím ảo (mạng mở /
// đã lưu thì chỉ xác nhận). Dữ liệu từ context property `wifi` (WifiManager);
// chỉ cập nhật khi tab đang hiện (wifi.active).
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GroundCtrl

Item {
    id: page

    function signalColor(s) { return s >= 60 ? Theme.accent : (s >= 35 ? Theme.warn : Theme.danger) }

    function choose(net) {
        if (net.active || wifi.connectingSsid !== "")
            return
        if (net.known || !net.secure)
            connectAsk.openFor(net)
        else
            passDialog.openFor(net.ssid)
    }

    // ── không có nmcli / wpa_cli (Windows, máy không Wi-Fi) ─────────────────
    Text {
        visible: !wifi.available
        anchors.centerIn: parent
        width: parent.width * 0.7
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.WordWrap
        text: "Không tìm thấy công cụ quản lý Wi-Fi (nmcli / wpa_cli) trên máy này."
        color: Theme.sub; font.family: Theme.ui; font.pixelSize: 13
    }

    RowLayout {
        visible: wifi.available
        anchors.fill: parent
        spacing: 10

        // ── mạng đang dùng ──────────────────────────────────────────────────
        Rectangle {
            Layout.preferredWidth: 290
            Layout.fillHeight: true
            radius: 10
            color: Theme.panelSolid
            border.color: Theme.stroke

            ColumnLayout {
                anchors.fill: parent
                anchors.margins: 14
                spacing: 10

                Text { text: "ĐANG KẾT NỐI"; color: Theme.dim
                       font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 1.2 }

                RowLayout {
                    spacing: 12
                    SignalBars {
                        level: wifi.currentSsid !== "" ? wifi.signal : -1
                        barWidth: 7; maxHeight: 30
                    }
                    ColumnLayout {
                        spacing: 2
                        Layout.fillWidth: true
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: wifi.currentSsid !== "" ? wifi.currentSsid : "Chưa kết nối"
                            color: wifi.currentSsid !== "" ? Theme.text : Theme.dim
                            font.family: Theme.ui; font.pixelSize: 20; font.bold: true
                        }
                        Text {
                            text: wifi.currentSsid !== "" && wifi.signal >= 0 ? "Tín hiệu " + wifi.signal + "%" : "—"
                            color: wifi.currentSsid !== "" ? page.signalColor(wifi.signal) : Theme.dim
                            font.family: Theme.mono; font.pixelSize: 11
                        }
                    }
                }

                Rectangle { Layout.fillWidth: true; height: 1; color: Theme.strokeSoft }

                InfoLine { label: "Địa chỉ IP"; value: wifi.ipAddress !== "" ? wifi.ipAddress : "—" }
                InfoLine { label: "Quản lý bởi"; value: wifi.backendName }

                Item { Layout.fillHeight: true }

                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: "Đổi mạng sẽ ngắt Wi-Fi vài giây. Link MAVLink qua UDP/TCP (Wi-Fi) mất tạm thời — "
                          + "radio telemetry qua serial không bị ảnh hưởng."
                    color: Theme.dim; font.family: Theme.ui; font.pixelSize: 10
                }

                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 40
                    radius: 8
                    color: scanMa.pressed ? "#1f3a2e" : "transparent"
                    border.color: Theme.accent
                    opacity: wifi.scanning ? 0.6 : 1
                    Text {
                        anchors.centerIn: parent
                        text: wifi.scanning ? "ĐANG QUÉT…" : "↻  QUÉT LẠI"
                        color: Theme.accent; font.family: Theme.mono; font.pixelSize: 12; font.bold: true
                    }
                    MouseArea { id: scanMa; anchors.fill: parent; enabled: !wifi.scanning; onClicked: wifi.scan() }
                }
            }
        }

        // ── danh sách mạng ──────────────────────────────────────────────────
        ColumnLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            spacing: 6

            RowLayout {
                Layout.fillWidth: true
                Text { text: "MẠNG XUNG QUANH"; color: Theme.dim
                       font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 1.2 }
                Text { text: wifi.networks.length > 0 ? "· " + wifi.networks.length : ""
                       color: Theme.dim; font.family: Theme.mono; font.pixelSize: 10 }
                Item { Layout.fillWidth: true }
                BusyDots { visible: wifi.scanning }
            }

            ListView {
                id: list
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                spacing: 6
                model: wifi.networks
                boundsBehavior: Flickable.StopAtBounds

                delegate: Rectangle {
                    id: row
                    required property var modelData
                    readonly property bool connecting: wifi.connectingSsid === modelData.ssid
                    width: ListView.view.width
                    height: 48
                    radius: 8
                    color: modelData.active ? "#122a20" : (rowMa.pressed ? "#18201b" : Theme.panel)
                    border.color: modelData.active || connecting ? Theme.accent : Theme.strokeBtn

                    RowLayout {
                        anchors.fill: parent
                        anchors.leftMargin: 14; anchors.rightMargin: 12
                        spacing: 12
                        SignalBars { level: row.modelData.signal }
                        Text {
                            Layout.fillWidth: true
                            elide: Text.ElideRight
                            text: row.modelData.ssid
                            color: Theme.text; font.family: Theme.ui; font.pixelSize: 15
                            font.bold: row.modelData.active
                        }
                        LockIcon { visible: row.modelData.secure }
                        Chip {
                            visible: row.connecting || row.modelData.active || row.modelData.known
                            text: row.connecting ? "ĐANG KẾT NỐI…" : (row.modelData.active ? "ĐANG DÙNG" : "ĐÃ LƯU")
                            tint: row.connecting || row.modelData.active ? Theme.accent : Theme.sub
                        }
                    }
                    MouseArea { id: rowMa; anchors.fill: parent; onClicked: page.choose(row.modelData) }
                }

                Text {
                    visible: list.count === 0
                    anchors.centerIn: parent
                    text: wifi.scanning ? "Đang quét mạng…" : "Không thấy mạng nào — bấm QUÉT LẠI."
                    color: Theme.dim; font.family: Theme.ui; font.pixelSize: 12
                }
            }

            Text {
                Layout.fillWidth: true
                elide: Text.ElideRight
                visible: wifi.message !== ""
                text: wifi.message
                color: wifi.messageError ? Theme.dangerSoft : Theme.accent
                font.family: Theme.ui; font.pixelSize: 11
            }
        }
    }

    // ── xác nhận mạng mở / đã lưu ───────────────────────────────────────────
    Popup {
        id: connectAsk
        property var net: ({})
        function openFor(n) { net = n; open() }
        parent: Overlay.overlay
        anchors.centerIn: parent
        width: 420
        modal: true
        padding: 18
        background: Rectangle { radius: 12; color: "#fa0e1210"; border.color: Theme.stroke }
        contentItem: ColumnLayout {
            spacing: 14
            Text { text: "Kết nối \"" + connectAsk.net.ssid + "\"?"; color: Theme.text
                   font.family: Theme.ui; font.pixelSize: 16; font.bold: true
                   Layout.fillWidth: true; elide: Text.ElideRight }
            Text {
                Layout.fillWidth: true
                wrapMode: Text.WordWrap
                text: connectAsk.net.known ? "Mạng đã lưu — dùng lại mật khẩu cũ." : "Mạng mở, không cần mật khẩu."
                color: Theme.sub; font.family: Theme.ui; font.pixelSize: 12
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                DialogBtn {
                    visible: connectAsk.net.known === true && connectAsk.net.secure === true
                    text: "NHẬP MẬT KHẨU MỚI"
                    onClicked: { connectAsk.close(); passDialog.openFor(connectAsk.net.ssid) }
                }
                Item { Layout.fillWidth: true }
                DialogBtn { text: "HUỶ"; onClicked: connectAsk.close() }
                DialogBtn {
                    text: "KẾT NỐI"; primary: true
                    onClicked: { wifi.connectTo(connectAsk.net.ssid, ""); connectAsk.close() }
                }
            }
        }
    }

    // ── nhập mật khẩu + bàn phím ảo ─────────────────────────────────────────
    Popup {
        id: passDialog
        property string ssid
        property bool reveal: false
        function openFor(s) { ssid = s; reveal = false; pass.text = ""; open(); pass.forceActiveFocus() }
        function submit() {
            if (pass.text.length < 8) return
            wifi.connectTo(ssid, pass.text)
            close()
        }

        parent: Overlay.overlay
        x: Math.round((parent.width - width) / 2)
        y: Math.max(4, Math.round((parent.height - height) / 2))
        width: Math.min(parent.width - 16, 760)
        modal: true
        padding: 12
        background: Rectangle { radius: 12; color: "#fc0b0f0d"; border.color: Theme.stroke }

        contentItem: ColumnLayout {
            spacing: 8
            RowLayout {
                Layout.fillWidth: true
                Text { text: "Mật khẩu cho"; color: Theme.sub; font.family: Theme.ui; font.pixelSize: 13 }
                Text { text: passDialog.ssid; color: Theme.text; font.family: Theme.ui; font.pixelSize: 15; font.bold: true
                       Layout.fillWidth: true; elide: Text.ElideRight }
                DialogBtn { text: "✕"; onClicked: passDialog.close() }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 8
                TextField {
                    id: pass
                    Layout.fillWidth: true
                    Layout.preferredHeight: 42
                    echoMode: passDialog.reveal ? TextInput.Normal : TextInput.Password
                    placeholderText: "ít nhất 8 ký tự"
                    color: Theme.text
                    placeholderTextColor: Theme.dim
                    font.family: Theme.mono; font.pixelSize: 17
                    leftPadding: 12
                    background: Rectangle { radius: 8; color: Theme.bg
                                            border.color: pass.activeFocus ? Theme.accent : Theme.stroke }
                    onAccepted: passDialog.submit()
                }
                DialogBtn {
                    text: passDialog.reveal ? "ẨN" : "HIỆN"
                    Layout.preferredHeight: 42
                    onClicked: passDialog.reveal = !passDialog.reveal
                }
            }
            OnScreenKeyboard {
                Layout.fillWidth: true
                target: pass
                okText: "KẾT NỐI"
                okEnabled: pass.text.length >= 8
                onAccepted: passDialog.submit()
            }
        }
    }

    // ── thành phần nhỏ ──────────────────────────────────────────────────────
    component SignalBars: Row {
        id: bars
        property int level: -1          // 0..100, -1 = không có
        property int barWidth: 5
        property int maxHeight: 18
        spacing: 2
        Repeater {
            model: 4
            delegate: Rectangle {
                required property int index
                anchors.bottom: parent.bottom
                width: bars.barWidth
                height: bars.maxHeight * (index + 1) / 4
                radius: 1
                color: bars.level >= 0 && bars.level >= index * 25 + 5 ? page.signalColor(bars.level) : "#2a332d"
            }
        }
    }

    component LockIcon: Item {
        implicitWidth: 12; implicitHeight: 15
        Rectangle { x: 2; y: 0; width: 8; height: 9; radius: 4; color: "transparent"
                    border.color: Theme.sub; border.width: 1.5 }
        Rectangle { x: 0; y: 6; width: 12; height: 9; radius: 2; color: Theme.sub }
    }

    component Chip: Rectangle {
        property alias text: chipTxt.text
        property color tint: Theme.sub
        implicitWidth: chipTxt.implicitWidth + 14; implicitHeight: 22; radius: 11
        color: "transparent"; border.color: tint
        Text { id: chipTxt; anchors.centerIn: parent; color: parent.tint
               font.family: Theme.mono; font.pixelSize: 9; font.bold: true }
    }

    component InfoLine: RowLayout {
        property string label
        property string value
        Layout.fillWidth: true
        Text { text: label; color: Theme.sub; font.family: Theme.ui; font.pixelSize: 12; Layout.fillWidth: true }
        Text { text: value; color: Theme.text; font.family: Theme.mono; font.pixelSize: 12 }
    }

    component DialogBtn: Rectangle {
        id: db
        property string text
        property bool primary: false
        signal clicked()
        implicitWidth: dbTxt.implicitWidth + 26; implicitHeight: 38; radius: 8
        color: primary ? (dbMa.pressed ? "#2fbf88" : Theme.accent) : (dbMa.pressed ? "#1d2520" : Theme.panel)
        border.color: primary ? Theme.accent : Theme.strokeBtn
        Text { id: dbTxt; anchors.centerIn: parent; text: db.text
               color: db.primary ? "#04140d" : Theme.text
               font.family: Theme.mono; font.pixelSize: 12; font.bold: true }
        MouseArea { id: dbMa; anchors.fill: parent; onClicked: db.clicked() }
    }

    component BusyDots: Row {
        spacing: 4
        Repeater {
            model: 3
            delegate: Rectangle {
                required property int index
                width: 6; height: 6; radius: 3; color: Theme.accent
                SequentialAnimation on opacity {
                    loops: Animation.Infinite
                    PauseAnimation { duration: index * 160 }
                    NumberAnimation { from: 0.2; to: 1; duration: 320 }
                    NumberAnimation { from: 1; to: 0.2; duration: 320 }
                    PauseAnimation { duration: (2 - index) * 160 }
                }
            }
        }
    }
}
