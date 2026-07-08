// Thanh chế độ bay (phải, 134px) — tiêu đề CHẾ ĐỘ BAY + các nút chế độ nhanh
// từ backend.quickModes. Nút đang khớp telemetry.modeLabel sáng đỏ như mockup.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GroundCtrl

Rectangle {
    id: root
    color: Theme.panelSolid

    // độ cao cất cánh mặc định (m) — hợp vùng bay hẹp. Người dùng chỉnh trong hộp
    // thoại khi bấm CẤT CÁNH; giá trị chọn được nhớ cho lần sau.
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
            text: "CHẾ ĐỘ BAY"; color: Theme.dim
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
            glyph: "▲"; label: "CẤT CÁNH"; sub: "chạm để chỉnh"; tint: Theme.accent
            onActivated: takeoffDlg.open()
        }
        ActionBtn {
            glyph: "▼"; label: "HẠ CÁNH"; sub: "hạ cánh & tắt động cơ"; tint: Theme.warn
            onActivated: backend.setMode("LAND")
        }

        // ── công tắc cần lái ảo (joystick) ──────────────────────────────────
        // Bật/tắt điều khiển bằng cần lái ảo. Khi bật, hai cần hiện trên vùng xem
        // và trạm bơm MANUAL_CONTROL ~25 Hz tới phương tiện. Sáng lục khi đang bật.
        Rectangle {
            id: joyToggle
            readonly property bool on: joystick.enabled
            Layout.fillWidth: true
            Layout.preferredHeight: 40
            radius: 6
            color: on ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.20)
                      : Theme.panel
            border.color: on ? Theme.accent : Theme.strokeBtn
            opacity: telemetry.connected ? 1 : 0.4

            Row {
                anchors.left: parent.left; anchors.leftMargin: 9
                anchors.verticalCenter: parent.verticalCenter
                spacing: 7
                Text {
                    text: "⌖"; color: joyToggle.on ? Theme.accent : Theme.sub
                    font.pixelSize: 16; font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
                Column {
                    anchors.verticalCenter: parent.verticalCenter
                    Text {
                        text: "CẦN LÁI"; color: joyToggle.on ? Theme.accent : Theme.sub
                        font.family: Theme.mono; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.5
                    }
                    Text {
                        text: joyToggle.on ? "đang bật · chạm để tắt" : "chạm để bật cần lái ảo"
                        color: Theme.dim; font.family: Theme.ui; font.pixelSize: 8
                    }
                }
            }

            // đèn báo trạng thái (phải)
            Rectangle {
                anchors.right: parent.right; anchors.rightMargin: 9
                anchors.verticalCenter: parent.verticalCenter
                width: 9; height: 9; radius: 4.5
                color: joyToggle.on ? Theme.accent : Theme.strokeBtn
            }

            MouseArea {
                anchors.fill: parent
                enabled: telemetry.connected
                cursorShape: Qt.PointingHandCursor
                onClicked: joystick.enabled = !joystick.enabled
            }
        }
    }

    // ── hộp thoại nhập độ cao cất cánh ──────────────────────────────────────
    // Bấm CẤT CÁNH mở hộp thoại này để nhập độ cao mục tiêu (m) thủ công thay vì
    // dùng giá trị cố định; xác nhận mới gửi lệnh backend.takeoff(alt).
    Popup {
        id: takeoffDlg
        parent: Overlay.overlay
        anchors.centerIn: parent
        modal: true
        dim: true
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        padding: 0
        width: 300

        // độ cao đang chọn (m); mở hộp thoại thì lấy giá trị đã nhớ
        property int alt: root.takeoffAlt
        onOpened: alt = root.takeoffAlt

        background: Rectangle {
            radius: 12
            color: "#f20a0d0b"
            border.color: Theme.accent
        }

        contentItem: ColumnLayout {
            spacing: 14

            Text {
                Layout.topMargin: 18; Layout.leftMargin: 20; Layout.rightMargin: 20
                text: "▲ CẤT CÁNH"
                color: Theme.accent; font.family: Theme.mono; font.pixelSize: 18; font.bold: true; font.letterSpacing: 1
            }
            Text {
                Layout.leftMargin: 20; Layout.rightMargin: 20
                text: "Nhập độ cao mục tiêu (mét)"
                color: Theme.dim; font.family: Theme.ui; font.pixelSize: 11
            }

            // stepper: −  [ giá trị ]  +   m
            RowLayout {
                Layout.alignment: Qt.AlignHCenter
                spacing: 10

                StepBtn { text: "−"; onClicked: takeoffDlg.alt = Math.max(1, takeoffDlg.alt - 1) }

                Rectangle {
                    Layout.preferredWidth: 96; Layout.preferredHeight: 46
                    radius: 8; color: Theme.panel; border.color: Theme.stroke
                    TextInput {
                        id: altInput
                        anchors.centerIn: parent
                        width: parent.width - 14
                        text: takeoffDlg.alt
                        color: Theme.text
                        font.family: Theme.mono; font.pixelSize: 24; font.bold: true
                        horizontalAlignment: TextInput.AlignHCenter
                        validator: IntValidator { bottom: 1; top: 500 }
                        selectByMouse: true
                        // onTextEdited chỉ chạy khi người dùng gõ (không lặp với bind ở trên)
                        onTextEdited: takeoffDlg.alt = Math.max(1, Math.min(500, parseInt(text) || 1))
                    }
                }

                StepBtn { text: "+"; onClicked: takeoffDlg.alt = Math.min(500, takeoffDlg.alt + 1) }

                Text { text: "m"; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 16 }
            }

            // nút HUỶ / CẤT CÁNH
            RowLayout {
                Layout.fillWidth: true
                Layout.leftMargin: 20; Layout.rightMargin: 20; Layout.bottomMargin: 18
                spacing: 10

                DlgBtn {
                    label: "HUỶ"; tint: "#8a938d"; filled: false
                    onActivated: takeoffDlg.close()
                }
                DlgBtn {
                    label: "CẤT CÁNH"; tint: Theme.accent; filled: true
                    onActivated: {
                        root.takeoffAlt = takeoffDlg.alt;   // nhớ cho lần sau
                        backend.takeoff(takeoffDlg.alt);
                        takeoffDlg.close();
                    }
                }
            }
        }
    }

    // nút hành động lớn (CẤT CÁNH / HẠ CÁNH) — tô theo tint, chỉ bấm khi đã kết nối
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

    // nút cộng/trừ độ cao trong hộp thoại
    component StepBtn: Rectangle {
        property alias text: lbl.text
        signal clicked()
        Layout.preferredWidth: 46; Layout.preferredHeight: 46
        radius: 8
        color: ma.pressed ? Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, 0.22) : Theme.panel
        border.color: Theme.strokeBtn
        Text { id: lbl; anchors.centerIn: parent; color: Theme.accent
               font.family: Theme.mono; font.pixelSize: 24; font.bold: true }
        MouseArea { id: ma; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: parent.clicked() }
    }

    // nút hành động trong hộp thoại (HUỶ / CẤT CÁNH)
    component DlgBtn: Rectangle {
        property string label
        property color tint: Theme.accent
        property bool filled: false
        signal activated()
        Layout.fillWidth: true
        Layout.preferredHeight: 40
        radius: 8
        color: filled ? Qt.rgba(tint.r, tint.g, tint.b, dma.pressed ? 0.34 : 0.18) : "transparent"
        border.color: tint
        Text { anchors.centerIn: parent; text: label; color: tint
               font.family: Theme.mono; font.pixelSize: 13; font.bold: true; font.letterSpacing: 0.6 }
        MouseArea { id: dma; anchors.fill: parent; cursorShape: Qt.PointingHandCursor; onClicked: parent.activated() }
    }
}
