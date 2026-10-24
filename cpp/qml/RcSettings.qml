// Trang cài đặt tay điều khiển RC — phủ lên vùng giữa khi bấm ⚙ trên thanh trạng thái.
//
// Ba tab: CÔNG TẮC (giám sát SA..SD + gán chế độ bay cho từng vị trí) · HIỆU CHỈNH
// (gimbal 2 cần, raw ADC, quy trình min/max/tâm → GHI → LƯU FLASH) · KÊNH (CH1..CH8).
// Số liệu từ context property `rc` (RcViewModel). Nút ≥ 34px logic (~50px thật
// trên màn 1280×720) cho dễ chạm.
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import GroundCtrl

Rectangle {
    id: root
    color: Theme.bg

    signal closeRequested()

    property string tab: "SW"
    readonly property var swNames: ["SA", "SB", "SC", "SD"]
    readonly property var posNames: ["LOW", "MID", "HIGH"]
    readonly property var adcNames: ["LH", "LV", "RH", "RV", "SB", "SC"]
    // SA/SD 2 vị trí (LOW/HIGH), SB/SC 3 vị trí — liệt kê từ trên xuống
    function positionsOf(sw) { return (sw === 0 || sw === 3) ? [2, 0] : [2, 1, 0] }

    // chặn chạm xuyên xuống bản đồ
    MouseArea { anchors.fill: parent }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // ── tiêu đề: tab + tình trạng link + đóng ───────────────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "RC SETUP"; color: Theme.text
                font.family: Theme.ui; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1.5
                Layout.rightMargin: 6
            }

            Repeater {
                model: [ { k: "SW", t: "CÔNG TẮC" }, { k: "CAL", t: "HIỆU CHỈNH" }, { k: "CH", t: "KÊNH" } ]
                delegate: Rectangle {
                    required property var modelData
                    readonly property bool sel: root.tab === modelData.k
                    implicitWidth: tabTxt.implicitWidth + 24; implicitHeight: 34; radius: 6
                    color: sel ? Theme.accent : Theme.panel
                    border.color: sel ? Theme.accent : Theme.strokeBtn
                    Text { id: tabTxt; anchors.centerIn: parent; text: modelData.t
                           color: parent.sel ? "#04140d" : Theme.text
                           font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
                    MouseArea { anchors.fill: parent; onClicked: root.tab = modelData.k }
                }
            }

            Item { Layout.fillWidth: true }

            // tình trạng link RC (đỏ to khi mất kết nối — guide §5)
            Rectangle {
                implicitHeight: 34; implicitWidth: linkRow.implicitWidth + 18; radius: 6
                color: rc.connected ? "#0e2018" : "#2a1113"
                border.color: rc.connected ? Theme.accent : Theme.danger
                Row {
                    id: linkRow
                    anchors.centerIn: parent
                    spacing: 8
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        text: rc.connected ? "● RC" : (rc.portOpen ? "○ RC MẤT TÍN HIỆU" : "○ RC CHƯA MỞ CỔNG")
                        color: rc.connected ? Theme.accent : Theme.dangerSoft
                        font.family: Theme.mono; font.pixelSize: 11; font.bold: true
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: rc.connected
                        text: rc.fps + " fps · drop " + rc.dropped
                              + " · CAL " + (rc.calibLoaded ? "✓" : "✗")
                              + " · ELRS " + (rc.elrsOutput ? "✓" : "✗")
                        color: Theme.text; font.family: Theme.mono; font.pixelSize: 10
                    }
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        visible: !rc.connected
                        text: rc.portName === "" ? "(chưa cấu hình cổng)" : rc.portName
                        color: Theme.dim; font.family: Theme.mono; font.pixelSize: 10
                    }
                }
            }

            Rectangle {
                implicitWidth: 34; implicitHeight: 34; radius: 6
                color: "transparent"; border.color: Theme.stroke
                Text { anchors.centerIn: parent; text: "✕"; color: Theme.danger
                       font.family: Theme.mono; font.pixelSize: 15; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.closeRequested() }
            }
        }

        // ── nội dung tab ────────────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: root.tab === "SW" ? 0 : (root.tab === "CAL" ? 1 : 2)

            // ════════ CÔNG TẮC ══════════════════════════════════════════════
            ColumnLayout {
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 10
                    Toggle {
                        checked: rc.modeSwitchEnabled
                        label: "GÁN MODE THEO CÔNG TẮC"
                        onToggled: rc.modeSwitchEnabled = !rc.modeSwitchEnabled
                    }
                    Text {
                        Layout.fillWidth: true
                        text: rc.lastAction !== "" ? "Lần cuối: " + rc.lastAction
                              : "Chạm ô chế độ bên phải mỗi vị trí để gán. Gạt công tắc ⇒ gửi đổi mode."
                        color: rc.lastAction !== "" ? Theme.accent : Theme.dim
                        font.family: Theme.ui; font.pixelSize: 10; elide: Text.ElideRight
                    }
                    Text {
                        visible: !telemetry.connected
                        text: "MAVLINK CHƯA KẾT NỐI"; color: Theme.warn
                        font.family: Theme.mono; font.pixelSize: 10; font.bold: true
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 8

                    Repeater {
                        model: 4
                        delegate: Rectangle {
                            id: card
                            required property int index
                            readonly property int cur: rc.switchPos[index]
                            Layout.fillWidth: true
                            Layout.fillHeight: true
                            radius: 8
                            color: Theme.panelSolid
                            border.color: Theme.stroke

                            ColumnLayout {
                                anchors.fill: parent
                                anchors.margins: 8
                                spacing: 6

                                RowLayout {
                                    Layout.fillWidth: true
                                    Text { text: root.swNames[card.index]; color: Theme.text
                                           font.family: Theme.mono; font.pixelSize: 18; font.bold: true }
                                    Text { text: (card.index === 0 || card.index === 3) ? "2 VỊ TRÍ" : "3 VỊ TRÍ"
                                           color: Theme.dim; font.family: Theme.mono; font.pixelSize: 9
                                           Layout.alignment: Qt.AlignBottom; Layout.bottomMargin: 3 }
                                    Item { Layout.fillWidth: true }
                                    Text {
                                        text: rc.connected && card.cur >= 0 ? root.posNames[card.cur] : "—"
                                        color: Theme.accent; font.family: Theme.mono; font.pixelSize: 14; font.bold: true
                                    }
                                }

                                Repeater {
                                    model: root.positionsOf(card.index)
                                    delegate: Rectangle {
                                        id: posRow
                                        required property int modelData
                                        readonly property bool active: rc.connected && card.cur === modelData
                                        readonly property string mode: rc.switchModes[card.index][modelData]
                                        Layout.fillWidth: true
                                        Layout.fillHeight: true
                                        radius: 6
                                        color: active ? "#1637e0a0" : Theme.panel
                                        border.color: active ? Theme.accent : Theme.strokeBtn

                                        RowLayout {
                                            anchors.fill: parent
                                            anchors.leftMargin: 8; anchors.rightMargin: 6
                                            spacing: 6
                                            Rectangle {
                                                width: 12; height: 12; radius: 6
                                                color: posRow.active ? Theme.accent : "transparent"
                                                border.color: posRow.active ? Theme.accent : "#4a534d"
                                            }
                                            Text {
                                                text: root.posNames[posRow.modelData]
                                                color: posRow.active ? Theme.text : Theme.sub
                                                font.family: Theme.mono; font.pixelSize: 11; font.bold: true
                                                Layout.preferredWidth: 34
                                            }
                                            Rectangle {
                                                Layout.fillWidth: true
                                                Layout.preferredHeight: 34
                                                radius: 5
                                                color: posRow.mode !== "" ? "#1aff3b3b" : "transparent"
                                                border.color: posRow.mode !== "" ? Theme.danger : Theme.stroke
                                                Text {
                                                    anchors.centerIn: parent
                                                    width: parent.width - 8
                                                    horizontalAlignment: Text.AlignHCenter
                                                    elide: Text.ElideRight
                                                    text: posRow.mode !== "" ? posRow.mode : "+ GÁN"
                                                    color: posRow.mode !== "" ? "#ffe9e9" : Theme.dim
                                                    font.family: Theme.mono; font.pixelSize: 11; font.bold: posRow.mode !== ""
                                                }
                                                MouseArea {
                                                    anchors.fill: parent
                                                    onClicked: modePicker.openFor(card.index, posRow.modelData)
                                                }
                                            }
                                        }
                                    }
                                }
                            }
                        }
                    }
                }
            }

            // ════════ HIỆU CHỈNH ════════════════════════════════════════════
            RowLayout {
                spacing: 10

                // hai gimbal (dot = (H, -V)), số µs dưới mỗi trục
                ColumnLayout {
                    Layout.fillHeight: true
                    spacing: 6
                    RowLayout {
                        spacing: 10
                        Gimbal { title: "TRÁI"; h: rc.controls[0]; v: rc.controls[1]; hName: "LH"; vName: "LV" }
                        Gimbal { title: "PHẢI"; h: rc.controls[2]; v: rc.controls[3]; hName: "RH"; vName: "RV" }
                    }
                    Item { Layout.fillHeight: true }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 4

                    // bảng raw ADC
                    GridLayout {
                        Layout.fillWidth: true
                        columns: 6
                        rowSpacing: 2
                        columnSpacing: 6
                        Repeater {
                            model: ["KÊNH", "RAW", "MIN", "TÂM", "MAX", "ĐANG LƯU"]
                            delegate: Text { required property string modelData
                                text: modelData; color: Theme.dim
                                font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1
                                Layout.fillWidth: true }
                        }
                        Repeater {
                            model: 6 * 6
                            delegate: Text {
                                required property int index
                                readonly property int row: Math.floor(index / 6)
                                readonly property int col: index % 6
                                readonly property bool live: rc.calStep > 0
                                readonly property var stored: rc.storedCal[row]
                                Layout.fillWidth: true
                                font.family: Theme.mono; font.pixelSize: 12
                                font.bold: col === 0
                                color: col === 0 ? Theme.text
                                       : (col === 1 ? Theme.accent
                                          : (col === 5 ? Theme.sub : (live ? Theme.warn : Theme.dim)))
                                text: col === 0 ? root.adcNames[row]
                                      : col === 1 ? rc.rawAdc[row]
                                      : col === 2 ? (live ? rc.calMin[row] : "—")
                                      : col === 3 ? (rc.calStep === 2 ? rc.calMid[row] : "—")
                                      : col === 4 ? (live ? rc.calMax[row] : "—")
                                      : (stored && stored.length === 3 ? stored[0] + "/" + stored[1] + "/" + stored[2] : "?")
                            }
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: rc.calMessage !== "" ? rc.calMessage
                              : "Hiệu chỉnh: BẮT ĐẦU → đẩy hết hành trình → về tâm → ĐẶT TÂM → GHI → LƯU FLASH."
                        color: rc.calMessageError ? Theme.dangerSoft : Theme.sub
                        font.family: Theme.ui; font.pixelSize: 11
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        ActBtn { label: "BẮT ĐẦU"; tint: Theme.accent
                                 enabled: rc.connected && rc.calStep === 0 && !rc.busy
                                 onActivated: rc.startCalibration() }
                        ActBtn { label: "ĐẶT TÂM"; tint: Theme.accent
                                 enabled: rc.calStep === 1
                                 onActivated: rc.captureCenter() }
                        ActBtn { label: "GHI"; tint: Theme.warn
                                 enabled: rc.calStep === 2 && !rc.busy
                                 onActivated: rc.applyCalibration() }
                        ActBtn { label: "HUỶ"; tint: Theme.sub
                                 enabled: rc.calStep > 0 && !rc.busy
                                 onActivated: rc.cancelCalibration() }
                        Item { Layout.fillWidth: true }
                        ActBtn { label: "LƯU FLASH"; tint: Theme.danger
                                 enabled: rc.connected && rc.calStep === 0 && !rc.busy
                                 onActivated: rc.saveConfig() }
                    }
                }
            }

            // ════════ KÊNH ══════════════════════════════════════════════════
            ColumnLayout {
                spacing: 4
                Repeater {
                    model: 8
                    delegate: RowLayout {
                        required property int index
                        readonly property int us: rc.channels[index]
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 10
                        Text { text: "CH" + (index + 1); color: Theme.text
                               font.family: Theme.mono; font.pixelSize: 13; font.bold: true
                               Layout.preferredWidth: 40 }
                        Text { text: rc.channelSources[index]; color: Theme.dim
                               font.family: Theme.mono; font.pixelSize: 11
                               Layout.preferredWidth: 26 }
                        Rectangle {
                            id: track
                            Layout.fillWidth: true
                            Layout.preferredHeight: 18
                            radius: 3
                            color: Theme.panel; border.color: Theme.strokeBtn
                            readonly property real f: Math.max(0, Math.min(1, (parent.us - 1000) / 1000))
                            // thanh từ 1500 tới giá trị hiện tại
                            Rectangle {
                                y: 2; height: parent.height - 4; radius: 2
                                x: Math.min(parent.width * 0.5, parent.width * parent.f)
                                width: Math.abs(parent.width * parent.f - parent.width * 0.5)
                                color: rc.connected ? Theme.accent : Theme.dim
                                opacity: 0.8
                            }
                            // vạch 1500
                            Rectangle { x: parent.width / 2 - 1; width: 2; height: parent.height; color: Theme.sub }
                        }
                        Text { text: rc.connected ? parent.us : "—"; color: Theme.text
                               font.family: Theme.mono; font.pixelSize: 13
                               horizontalAlignment: Text.AlignRight
                               Layout.preferredWidth: 44 }
                    }
                }
            }
        }
    }

    // ── chọn chế độ cho một vị trí công tắc ─────────────────────────────────
    Popup {
        id: modePicker
        property int sw: 0
        property int pos: 0
        function openFor(s, p) { sw = s; pos = p; open() }

        parent: root
        anchors.centerIn: parent
        width: Math.min(root.width - 40, 560)
        modal: true
        padding: 14
        background: Rectangle { radius: 10; color: "#fa0e1210"; border.color: Theme.stroke }

        contentItem: ColumnLayout {
            spacing: 10
            Text {
                text: root.swNames[modePicker.sw] + " · " + root.posNames[modePicker.pos] + "  →  chọn chế độ bay"
                color: Theme.text; font.family: Theme.ui; font.pixelSize: 13; font.bold: true
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 4
                rowSpacing: 6; columnSpacing: 6
                Repeater {
                    model: [""].concat(rc.availableModes)
                    delegate: Rectangle {
                        required property string modelData
                        readonly property bool sel: rc.switchModes[modePicker.sw][modePicker.pos] === modelData
                        Layout.fillWidth: true
                        Layout.preferredHeight: 38
                        radius: 6
                        color: sel ? "#33ff3b3b" : Theme.panel
                        border.color: sel ? Theme.danger : Theme.strokeBtn
                        Text { anchors.centerIn: parent
                               text: modelData === "" ? "— KHÔNG" : modelData
                               color: modelData === "" ? Theme.dim : Theme.text
                               font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
                        MouseArea {
                            anchors.fill: parent
                            onClicked: { rc.setSwitchMode(modePicker.sw, modePicker.pos, modelData); modePicker.close() }
                        }
                    }
                }
            }
        }
    }

    // ── phần tử phụ ─────────────────────────────────────────────────────────
    component Toggle: Rectangle {
        property bool checked
        property string label
        signal toggled()
        implicitWidth: tgRow.implicitWidth + 20; implicitHeight: 34; radius: 6
        color: Theme.panel; border.color: checked ? Theme.accent : Theme.strokeBtn
        Row {
            id: tgRow
            anchors.centerIn: parent
            spacing: 8
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: 34; height: 18; radius: 9
                color: checked ? Theme.accent : "#2a312b"
                Rectangle { width: 14; height: 14; radius: 7; y: 2; x: checked ? 18 : 2; color: "#e9e7ee"
                            Behavior on x { NumberAnimation { duration: 120 } } }
            }
            Text { anchors.verticalCenter: parent.verticalCenter
                   text: label + (checked ? " · BẬT" : " · TẮT")
                   color: checked ? Theme.text : Theme.dim
                   font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
        }
        MouseArea { anchors.fill: parent; onClicked: parent.toggled() }
    }

    component ActBtn: Rectangle {
        property string label
        property color tint: Theme.accent
        signal activated()
        implicitWidth: Math.max(78, abTxt.implicitWidth + 22); implicitHeight: 36; radius: 6
        color: Qt.rgba(tint.r, tint.g, tint.b, enabled ? 0.16 : 0.05)
        border.color: enabled ? tint : Theme.strokeBtn
        Text { id: abTxt; anchors.centerIn: parent; text: label
               color: parent.enabled ? parent.tint : Theme.dim
               font.family: Theme.mono; font.pixelSize: 12; font.bold: true }
        MouseArea { anchors.fill: parent; enabled: parent.enabled; onClicked: parent.activated() }
    }

    component Gimbal: ColumnLayout {
        property string title
        property int h: 1500
        property int v: 1500
        property string hName
        property string vName
        spacing: 4
        Text { text: title; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1 }
        Rectangle {
            id: pad
            Layout.preferredWidth: 150; Layout.preferredHeight: 150
            radius: 8; color: Theme.panel; border.color: Theme.strokeBtn
            readonly property real nx: Math.max(-1, Math.min(1, (h - 1500) / 500))
            readonly property real ny: Math.max(-1, Math.min(1, (v - 1500) / 500))
            Rectangle { x: parent.width / 2; y: 6; width: 1; height: parent.height - 12; color: Theme.stroke }
            Rectangle { y: parent.height / 2; x: 6; height: 1; width: parent.width - 12; color: Theme.stroke }
            Rectangle {
                width: 16; height: 16; radius: 8
                color: rc.connected ? Theme.danger : Theme.dim
                x: parent.width / 2 + pad.nx * (parent.width / 2 - 10) - width / 2
                y: parent.height / 2 - pad.ny * (parent.height / 2 - 10) - height / 2
            }
        }
        Text { text: hName + " " + (rc.connected ? h : "—") + "   " + vName + " " + (rc.connected ? v : "—")
               color: Theme.text; font.family: Theme.mono; font.pixelSize: 11 }
    }
}
