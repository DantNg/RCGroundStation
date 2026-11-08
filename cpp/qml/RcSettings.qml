// Trang cài đặt tay điều khiển RC — phủ lên vùng giữa khi bấm ⚙ trên thanh trạng thái.
//
// Bốn tab: CÔNG TẮC (giám sát SA..SD + gán chế độ bay cho từng vị trí) · HIỆU CHỈNH
// (phiên hiệu chỉnh trên STM32: chọn kênh → BẮT ĐẦU → quét → CHỤP TÂM → ÁP DỤNG)
// · KÊNH (CH1..CH8 + đảo chiều) · CẤU HÌNH (cần lái qua radio / Pi, giá trị giữ,
// xuất RC, LƯU / BỎ THAY ĐỔI / MẶC ĐỊNH, thoát ứng dụng). Thao tác nguy hiểm đều hỏi xác nhận
// (PI_UI_AGENT_GUIDE.md §6). Số liệu từ context property `rc` (RcViewModel).
// Nút ≥ 34px logic (~50px thật trên màn 1280×720) cho dễ chạm.
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
    readonly property var stickNames: ["LH", "LV", "RH", "RV"]
    // chức năng CH1..8: CH1..4 theo chuẩn ArduPilot (RCMAP mặc định), CH5..8 kênh phụ
    readonly property var chFuncs: ["ROLL", "PITCH", "THROTTLE", "YAW", "AUX1", "AUX2", "AUX3", "AUX4"]
    readonly property var funcShort: ["ROLL", "PITCH", "THR", "YAW", "AUX1", "AUX2", "AUX3", "AUX4"]
    // Mọi hiển thị đi theo ánh xạ thật (SUB_MAP), không theo tên nguồn nội bộ:
    // nguồn đang cấp kênh ch, giá trị của nó, và chức năng của một nguồn.
    function srcOfCh(ch) { return rc.channelSources[ch] }
    function valOfCh(ch) {
        const i = rc.controlNames.indexOf(rc.channelSources[ch])
        return i >= 0 ? rc.controls[i] : 1500
    }
    function funcOfSrc(name) {
        const ch = rc.channelSources.indexOf(name)
        return ch >= 0 ? funcShort[ch] : "—"
    }
    // công tắc SA..SD (0..3) xếp theo kênh đang cấp (AUX1 trước), chưa gán xuống cuối
    readonly property var swOrder: [0, 1, 2, 3].sort((a, b) => {
        const ca = rc.channelSources.indexOf(swNames[a]), cb = rc.channelSources.indexOf(swNames[b])
        return (ca < 0 ? 99 : ca) - (cb < 0 ? 99 : cb)
    })
    // SA/SD 2 vị trí (LOW/HIGH), SB/SC 3 vị trí — liệt kê từ trên xuống
    function positionsOf(sw) { return (sw === 0 || sw === 3) ? [2, 0] : [2, 1, 0] }
    // kênh CRSF đang lấy nguồn `name` ("CH3") — để ghi chú giá trị giữ
    function channelOf(name) {
        const i = rc.channelSources.indexOf(name)
        return i >= 0 ? "CH" + (i + 1) : "—"
    }

    // chặn chạm xuyên xuống bản đồ
    MouseArea { anchors.fill: parent }

    ColumnLayout {
        anchors.fill: parent
        anchors.margins: 10
        spacing: 8

        // ── tiêu đề: tab + chưa lưu + tình trạng link + đóng ────────────────
        RowLayout {
            Layout.fillWidth: true
            spacing: 6

            Text {
                text: "RC SETUP"; color: Theme.text
                font.family: Theme.ui; font.pixelSize: 13; font.bold: true; font.letterSpacing: 1.5
                Layout.rightMargin: 4
            }

            Repeater {
                model: [ { k: "SW", t: "CÔNG TẮC" }, { k: "CAL", t: "HIỆU CHỈNH" },
                         { k: "CH", t: "KÊNH" }, { k: "CFG", t: "CẤU HÌNH" } ]
                delegate: Rectangle {
                    required property var modelData
                    readonly property bool sel: root.tab === modelData.k
                    implicitWidth: tabTxt.implicitWidth + 22; implicitHeight: 34; radius: 6
                    color: sel ? Theme.accent : Theme.panel
                    border.color: sel ? Theme.accent : Theme.strokeBtn
                    Text { id: tabTxt; anchors.centerIn: parent; text: modelData.t
                           color: parent.sel ? "#04140d" : Theme.text
                           font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
                    MouseArea { anchors.fill: parent; onClicked: root.tab = modelData.k }
                }
            }

            Item { Layout.fillWidth: true }

            // còn thay đổi chưa lưu flash (cờ dirty) — chạm để tới CẤU HÌNH
            Rectangle {
                visible: rc.connected && rc.dirty
                implicitHeight: 34; implicitWidth: dirtyTxt.implicitWidth + 18; radius: 6
                color: "#26ffb020"; border.color: Theme.warn
                Text { id: dirtyTxt; anchors.centerIn: parent; text: "● CHƯA LƯU"
                       color: Theme.warn; font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
                MouseArea { anchors.fill: parent; onClicked: root.tab = "CFG" }
            }

            // tình trạng link RC (đỏ to khi mất kết nối — guide §5)
            Rectangle {
                implicitHeight: 34; implicitWidth: linkTxt.implicitWidth + 18; radius: 6
                color: rc.connected ? "#0e2018" : "#2a1113"
                border.color: rc.connected ? Theme.accent : Theme.danger
                Text {
                    id: linkTxt
                    anchors.centerIn: parent
                    text: rc.connected ? "● RC " + rc.fps + " fps · drop " + rc.dropped
                          : (rc.portOpen ? "○ RC MẤT TÍN HIỆU"
                             : "○ RC CHƯA MỞ " + (rc.portName === "" ? "CỔNG" : rc.portName))
                    color: rc.connected ? Theme.accent : Theme.dangerSoft
                    font.family: Theme.mono; font.pixelSize: 11; font.bold: true
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

        // ── banner: cần lái không còn bay drone qua radio (§6.2) ────────────
        Rectangle {
            visible: rc.connected && !rc.stickCrsf
            Layout.fillWidth: true
            Layout.preferredHeight: 26
            radius: 5
            color: "#33ff3b3b"; border.color: Theme.danger
            Text {
                anchors.centerIn: parent
                text: "CẦN LÁI → PI · drone đang nhận GIÁ TRỊ GIỮ trên kênh cần lái, công tắc vẫn chạy"
                color: Theme.dangerSoft; font.family: Theme.mono; font.pixelSize: 11; font.bold: true
            }
            MouseArea { anchors.fill: parent; onClicked: root.tab = "CFG" }
        }

        // ── nội dung tab ────────────────────────────────────────────────────
        StackLayout {
            Layout.fillWidth: true
            Layout.fillHeight: true
            currentIndex: ["SW", "CAL", "CH", "CFG"].indexOf(root.tab)

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
                        model: root.swOrder
                        delegate: Rectangle {
                            id: card
                            required property int modelData
                            readonly property int index: modelData   // chỉ số công tắc SA..SD
                            readonly property int ch: rc.channelSources.indexOf(root.swNames[index])
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
                                    Text { text: card.ch >= 0 ? root.chFuncs[card.ch] : "—"; color: Theme.text
                                           font.family: Theme.mono; font.pixelSize: 18; font.bold: true }
                                    Text { text: (card.ch >= 0 ? "CH" + (card.ch + 1) + " ← " : "") + root.swNames[card.index]
                                                 + " · " + ((card.index === 0 || card.index === 3) ? "2" : "3") + " VT"
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

                // hai gimbal theo CHỨC NĂNG (Mode 2: trái YAW/THROTTLE, phải ROLL/PITCH)
                // lấy nguồn qua ánh xạ kênh nên luôn khớp tab KÊNH. PITCH vẽ ngược
                // (đẩy lên = PWM thấp, quy ước ArduPilot). ⇄ đổi chỗ hai gimbal (lưu lại).
                ColumnLayout {
                    Layout.fillHeight: true
                    spacing: 6
                    RowLayout {
                        spacing: 10
                        Gimbal { title: "TRÁI"; hCh: rc.gimbalSwap ? 0 : 3; vCh: rc.gimbalSwap ? 1 : 2 }
                        Gimbal { title: "PHẢI"; hCh: rc.gimbalSwap ? 3 : 0; vCh: rc.gimbalSwap ? 2 : 1 }
                    }
                    Toggle {
                        checked: rc.gimbalSwap
                        label: "⇄ ĐỔI TRÁI / PHẢI"
                        onToggled: rc.gimbalSwap = !rc.gimbalSwap
                    }
                    Item { Layout.fillHeight: true }
                }

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 3

                    // chọn nhanh nhóm kênh (chỉ khi chưa mở phiên)
                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        Repeater {
                            model: [ { m: 0x0F, t: "CẦN LÁI" }, { m: 0x30, t: "SB / SC" }, { m: 0x3F, t: "TẤT CẢ" } ]
                            delegate: Rectangle {
                                required property var modelData
                                readonly property bool sel: rc.calMask === modelData.m
                                implicitWidth: psTxt.implicitWidth + 20; implicitHeight: 32; radius: 6
                                color: sel ? "#1637e0a0" : Theme.panel
                                border.color: sel ? Theme.accent : Theme.strokeBtn
                                opacity: rc.calActive ? 0.4 : 1
                                Text { id: psTxt; anchors.centerIn: parent; text: modelData.t
                                       color: parent.sel ? Theme.accent : Theme.text
                                       font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
                                MouseArea { anchors.fill: parent; enabled: !rc.calActive
                                            onClicked: rc.setCalMask(modelData.m) }
                            }
                        }
                        Item { Layout.fillWidth: true }
                        Rectangle {
                            visible: rc.calActive
                            implicitWidth: sessTxt.implicitWidth + 16; implicitHeight: 26; radius: 5
                            color: "#26ffb020"; border.color: Theme.warn
                            Text { id: sessTxt; anchors.centerIn: parent; text: "● PHIÊN HIỆU CHỈNH ĐANG MỞ"
                                   color: Theme.warn; font.family: Theme.mono; font.pixelSize: 10; font.bold: true }
                        }
                    }

                    // bảng: chạm hàng để chọn/bỏ kênh (khi chưa mở phiên)
                    CalRow {
                        header: true
                        cells: ["", "KÊNH", "RAW", "MIN", "TÂM", "MAX", "HÀNH TRÌNH", "ĐANG DÙNG"]
                    }
                    Repeater {
                        model: 6
                        delegate: CalRow {
                            required property int index
                            readonly property bool sel: (rc.calMask & (1 << index)) !== 0
                            readonly property bool ranged: rc.calActive && (rc.calRangeMask & (1 << index)) !== 0
                            readonly property bool centered: rc.calActive && (rc.calCenterMask & (1 << index)) !== 0
                            readonly property int mn: rc.calValues[index * 3]
                            readonly property int mid: rc.calValues[index * 3 + 1]
                            readonly property int mx: rc.calValues[index * 3 + 2]
                            readonly property var stored: rc.storedCal[index]
                            selected: sel
                            dimmed: rc.calActive && !sel
                            span: ranged ? mx - mn : -1
                            onTapped: rc.toggleCalChannel(index)
                            cells: [
                                sel ? "☑" : "☐",
                                root.adcNames[index] + " " + root.funcOfSrc(root.adcNames[index]),
                                rc.rawAdc[index],
                                ranged ? mn : "—",
                                centered ? mid : (ranged ? "giữa" : "—"),
                                ranged ? mx : "—",
                                "",
                                stored && stored.length === 3 ? stored[0] + "/" + stored[1] + "/" + stored[2] : "?"
                            ]
                        }
                    }

                    Item { Layout.fillHeight: true }

                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        maximumLineCount: 3
                        text: rc.message !== "" ? rc.message
                              : "Chọn kênh cần chỉnh → BẮT ĐẦU → xoay/gạt hết hành trình → (thả cần về giữa, CHỤP TÂM) "
                                + "→ ÁP DỤNG → LƯU. Kênh không chọn giữ nguyên. STM32 dùng hiệu chỉnh cũ tới khi áp dụng."
                        color: rc.messageError ? Theme.dangerSoft : Theme.sub
                        font.family: Theme.ui; font.pixelSize: 11
                    }

                    RowLayout {
                        Layout.fillWidth: true
                        spacing: 6
                        ActBtn { label: "BẮT ĐẦU"; tint: Theme.accent
                                 enabled: rc.connected && !rc.calActive && rc.calMask !== 0 && !rc.busy
                                 onActivated: rc.startCalibration() }
                        ActBtn { label: "CHỤP TÂM"; tint: Theme.accent
                                 enabled: rc.calActive && (rc.calMask & 0x0F) !== 0 && !rc.busy
                                 onActivated: rc.captureCenter() }
                        ActBtn { label: "ÁP DỤNG"; tint: Theme.warn
                                 enabled: rc.calReady && !rc.busy
                                 onActivated: rc.applyCalibration() }
                        ActBtn { label: "HUỶ"; tint: Theme.sub
                                 enabled: rc.calActive && !rc.busy
                                 onActivated: rc.cancelCalibration() }
                        Item { Layout.fillWidth: true }
                        SaveBtn {}
                    }
                }
            }

            // ════════ KÊNH ══════════════════════════════════════════════════
            ColumnLayout {
                spacing: 3
                Repeater {
                    model: 8
                    delegate: RowLayout {
                        id: chRow
                        required property int index
                        readonly property int us: rc.channels[index]
                        readonly property string srcName: rc.channelSources[index]
                        // kênh do cần lái cấp + cần lái đang giao cho Pi ⇒ drone nhận giá trị giữ
                        readonly property bool held: !rc.stickCrsf && root.stickNames.indexOf(srcName) >= 0
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        spacing: 10
                        // chạm chức năng → gạt cần / công tắc muốn dùng → GÁN (learnPicker)
                        Rectangle {
                            readonly property bool usable: rc.connected && !rc.busy && !rc.calActive && !telemetry.armed
                            Layout.preferredWidth: 230
                            Layout.preferredHeight: 34
                            radius: 6
                            color: fnMa.pressed ? "#2637e0a0" : Theme.panel
                            border.color: Theme.strokeBtn
                            opacity: usable ? 1 : 0.5
                            RowLayout {
                                anchors.fill: parent
                                anchors.leftMargin: 10; anchors.rightMargin: 10
                                spacing: 6
                                Text { text: root.chFuncs[chRow.index]; color: Theme.text
                                       font.family: Theme.mono; font.pixelSize: 13; font.bold: true
                                       Layout.preferredWidth: 86 }
                                Text { text: "CH" + (chRow.index + 1); color: Theme.dim
                                       font.family: Theme.mono; font.pixelSize: 11
                                       Layout.preferredWidth: 30 }
                                Text { text: "← " + chRow.srcName; color: Theme.accent
                                       font.family: Theme.mono; font.pixelSize: 13; font.bold: true
                                       Layout.fillWidth: true }
                                Text { text: "▾"; color: Theme.sub; font.pixelSize: 12 }
                            }
                            MouseArea { id: fnMa; anchors.fill: parent; enabled: parent.usable
                                        onClicked: learnPicker.openFor(chRow.index) }
                        }
                        Rectangle {
                            Layout.fillWidth: true
                            Layout.preferredHeight: 18
                            radius: 3
                            color: Theme.panel; border.color: Theme.strokeBtn
                            readonly property real f: Math.max(0, Math.min(1, (chRow.us - 1000) / 1000))
                            // thanh từ 1500 tới giá trị hiện tại
                            Rectangle {
                                y: 2; height: parent.height - 4; radius: 2
                                x: Math.min(parent.width * 0.5, parent.width * parent.f)
                                width: Math.abs(parent.width * parent.f - parent.width * 0.5)
                                color: !rc.connected ? Theme.dim : (chRow.held ? Theme.warn : Theme.accent)
                                opacity: 0.8
                            }
                            // vạch 1500
                            Rectangle { x: parent.width / 2 - 1; width: 2; height: parent.height; color: Theme.sub }
                        }
                        Text { text: chRow.held ? "GIỮ" : ""; color: Theme.warn
                               font.family: Theme.mono; font.pixelSize: 10; font.bold: true
                               Layout.preferredWidth: 26 }
                        Text { text: rc.connected ? chRow.us : "—"; color: Theme.text
                               font.family: Theme.mono; font.pixelSize: 13
                               horizontalAlignment: Text.AlignRight
                               Layout.preferredWidth: 44 }

                        // đảo chiều nguồn điều khiển kênh này (REVERSE theo nguồn,
                        // ghi RAM STM32 ngay khi chạm — LƯU để giữ)
                        Rectangle {
                            readonly property int src: rc.controlNames.indexOf(chRow.srcName)
                            readonly property int rev: src >= 0 ? rc.reversed[src] : -1
                            readonly property bool usable: rc.connected && src >= 0 && rev >= 0 && !rc.busy
                            Layout.preferredWidth: 78
                            Layout.preferredHeight: 32
                            radius: 6
                            color: rev === 1 ? "#26ffb020" : Theme.panel
                            border.color: rev === 1 ? Theme.warn : Theme.strokeBtn
                            opacity: usable ? 1 : 0.4
                            Text { anchors.centerIn: parent
                                   text: parent.rev === 1 ? "⇅ ĐẢO" : (parent.rev === 0 ? "THƯỜNG" : "?")
                                   color: parent.rev === 1 ? Theme.warn : Theme.sub
                                   font.family: Theme.mono; font.pixelSize: 11; font.bold: true }
                            MouseArea { anchors.fill: parent; enabled: parent.usable
                                        onClicked: rc.setReverse(parent.src, parent.rev !== 1) }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        text: rc.message !== "" ? rc.message
                              : (telemetry.armed ? "Drone đang ARM — khoá gán kênh. "
                                                 : "Chạm một chức năng rồi gạt cần / công tắc muốn dùng cho nó. ")
                                + "THƯỜNG / ĐẢO đảo chiều nguồn của kênh. Thay đổi có hiệu lực ngay (RAM) — LƯU để giữ."
                        color: rc.messageError ? Theme.dangerSoft : Theme.sub
                        font.family: Theme.ui; font.pixelSize: 11
                    }
                    SaveBtn {}
                }
            }

            // ════════ CẤU HÌNH ══════════════════════════════════════════════
            ColumnLayout {
                spacing: 8

                RowLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    spacing: 10

                    // ── ai bay drone bằng cần lái (§6.2) ────────────────────
                    Card {
                        title: "CẦN LÁI ĐIỀU KHIỂN DRONE"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1

                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            Repeater {
                                model: [ { radio: true, t: "QUA RADIO", s: "mLRS · mặc định" },
                                         { radio: false, t: "QUA PI", s: "drone nhận giá trị giữ" } ]
                                delegate: Rectangle {
                                    required property var modelData
                                    readonly property bool cur: rc.connected && rc.stickCrsf === modelData.radio
                                    Layout.fillWidth: true
                                    Layout.preferredHeight: 50
                                    radius: 7
                                    color: cur ? (modelData.radio ? "#1637e0a0" : "#33ff3b3b") : Theme.panel
                                    border.color: cur ? (modelData.radio ? Theme.accent : Theme.danger) : Theme.strokeBtn
                                    opacity: rc.connected && !rc.busy ? 1 : 0.45
                                    Column {
                                        anchors.centerIn: parent
                                        spacing: 2
                                        Text { anchors.horizontalCenter: parent.horizontalCenter
                                               text: (parent.parent.cur ? "● " : "") + modelData.t
                                               color: parent.parent.cur ? Theme.text : Theme.sub
                                               font.family: Theme.mono; font.pixelSize: 13; font.bold: true }
                                        Text { anchors.horizontalCenter: parent.horizontalCenter
                                               text: modelData.s; color: Theme.dim
                                               font.family: Theme.ui; font.pixelSize: 9 }
                                    }
                                    MouseArea {
                                        anchors.fill: parent
                                        enabled: rc.connected && !rc.busy && !parent.cur
                                        onClicked: modelData.radio ? confirm.ask(
                                            "Trả cần lái cho RADIO?",
                                            "Drone sẽ nhận lại giá trị cần lái trực tiếp qua mLRS ngay lập tức."
                                            + (telemetry.armed ? "\n\n⚠ Drone đang ARM — cần lái phải ở vị trí an toàn trước khi chuyển." : ""),
                                            "CHUYỂN VỀ RADIO", Theme.accent, () => rc.setStickCrsf(true))
                                            : confirm.ask(
                                            "Giao cần lái cho PI?",
                                            "mLRS sẽ gửi GIÁ TRỊ GIỮ (bên dưới) trên các kênh cần lái; công tắc vẫn chạy.\n"
                                            + "LiteGCS hiện CHƯA điều khiển drone bằng cần lái qua telemetry — "
                                            + "cần lái sẽ KHÔNG còn lái được drone."
                                            + (telemetry.armed ? "\n\n⚠ Drone đang ARM / có thể đang bay." : ""),
                                            "GIAO CHO PI", Theme.danger, () => rc.setStickCrsf(false))
                                    }
                                }
                            }
                        }

                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "GIÁ TRỊ GIỮ — drone nhận khi cần lái giao cho Pi (và khi Pi mất điều khiển). "
                                  + "Giữ ga thấp trừ khi bạn chắc chắn."
                            color: Theme.dim; font.family: Theme.ui; font.pixelSize: 10
                        }

                        Repeater {
                            model: 4
                            delegate: Rectangle {
                                id: holdRow
                                required property int index
                                readonly property int val: rc.stickHold[index]
                                Layout.fillWidth: true
                                Layout.preferredHeight: 36
                                radius: 6
                                color: Theme.panel; border.color: Theme.strokeBtn
                                RowLayout {
                                    anchors.fill: parent
                                    anchors.leftMargin: 10; anchors.rightMargin: 4
                                    spacing: 8
                                    Text { text: root.stickNames[holdRow.index]; color: Theme.text
                                           font.family: Theme.mono; font.pixelSize: 13; font.bold: true
                                           Layout.preferredWidth: 28 }
                                    Text { text: root.channelOf(root.stickNames[holdRow.index]); color: Theme.dim
                                           font.family: Theme.mono; font.pixelSize: 11; Layout.preferredWidth: 34 }
                                    Text { text: holdRow.val >= 0 ? holdRow.val + " µs" : "?"; color: Theme.accent
                                           font.family: Theme.mono; font.pixelSize: 13; Layout.fillWidth: true }
                                    ActBtn { label: "SỬA"; tint: Theme.accent; implicitHeight: 28
                                             enabled: rc.connected && holdRow.val >= 0 && !rc.busy
                                             onActivated: holdEditor.openFor(holdRow.index, holdRow.val) }
                                }
                            }
                        }
                        Item { Layout.fillHeight: true }
                    }

                    // ── cấu hình STM32 (§6.4) ───────────────────────────────
                    Card {
                        title: "CẤU HÌNH STM32"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        Layout.preferredWidth: 1

                        InfoRow {
                            label: "Xuất RC ra radio (mLRS)"
                            value: !rc.connected ? "—" : (rc.elrsOutput ? "BẬT" : "TẮT")
                            valueColor: rc.elrsOutput ? Theme.accent : Theme.danger
                            ActBtn {
                                label: rc.elrsOutput ? "TẮT" : "BẬT"
                                tint: rc.elrsOutput ? Theme.danger : Theme.accent
                                implicitHeight: 30
                                enabled: rc.connected && !rc.busy
                                onActivated: rc.elrsOutput
                                    ? confirm.ask("TẮT toàn bộ RC ra radio?",
                                                  "Radio sẽ ngừng gửi mọi kênh — drone sẽ vào FAILSAFE.",
                                                  "TẮT XUẤT RC", Theme.danger, () => rc.setElrsOutput(false))
                                    : rc.setElrsOutput(true)
                            }
                        }
                        InfoRow { label: "Cấu hình nạp từ flash"; value: !rc.connected ? "—" : (rc.calibLoaded ? "CÓ" : "MẶC ĐỊNH")
                                  valueColor: rc.calibLoaded ? Theme.accent : Theme.warn }
                        InfoRow { label: "Ánh xạ cần lái"
                                  value: rc.stickTrim === 0 ? "CALIB 3 ĐIỂM" : (rc.stickTrim === 1 ? "TRIM CỐ ĐỊNH (CŨ)" : "?")
                                  valueColor: Theme.text }
                        InfoRow { label: "Thay đổi chưa lưu"; value: !rc.connected ? "—" : (rc.dirty ? "CÓ" : "KHÔNG")
                                  valueColor: rc.dirty ? Theme.warn : Theme.accent }

                        Item { Layout.fillHeight: true }

                        Text {
                            Layout.fillWidth: true
                            wrapMode: Text.WordWrap
                            text: "Mọi thay đổi có hiệu lực ngay trong RAM. LƯU ghi flash (STM32 và radio dừng thoáng chốc — "
                                  + "không lưu khi đang bay). BỎ THAY ĐỔI nạp lại từ flash."
                            color: Theme.dim; font.family: Theme.ui; font.pixelSize: 10
                        }
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: 6
                            SaveBtn {}
                            ActBtn { label: "BỎ THAY ĐỔI"; tint: Theme.warn
                                     enabled: rc.connected && rc.dirty && !rc.busy
                                     onActivated: confirm.ask("Bỏ thay đổi chưa lưu?",
                                                              "STM32 nạp lại cấu hình từ flash — mọi thay đổi chưa lưu sẽ mất.",
                                                              "BỎ THAY ĐỔI", Theme.warn, () => rc.reloadConfig()) }
                            ActBtn { label: "MẶC ĐỊNH"; tint: Theme.sub
                                     enabled: rc.connected && !rc.busy
                                     onActivated: confirm.ask("Về cấu hình mặc định?",
                                                              "Đưa cấu hình STM32 về mặc định nhà máy trong RAM (cần LƯU để giữ).",
                                                              "MẶC ĐỊNH, GIỮ CALIB", Theme.warn, () => rc.resetDefaults(true),
                                                              "XOÁ CẢ CALIB", () => rc.resetDefaults(false)) }
                        }
                    }
                }

                RowLayout {
                    Layout.fillWidth: true
                    spacing: 8
                    Text {
                        Layout.fillWidth: true
                        wrapMode: Text.WordWrap
                        maximumLineCount: 2
                        text: rc.message
                        color: rc.messageError ? Theme.dangerSoft : Theme.sub
                        font.family: Theme.ui; font.pixelSize: 11
                    }
                    // thoát ứng dụng — chỉ đặt trong cài đặt để không lỡ chạm khi đang bay
                    ActBtn { label: "⏻ THOÁT ỨNG DỤNG"; tint: Theme.danger
                             onActivated: confirm.ask("Thoát LiteGCS?",
                                                      "Đóng ứng dụng trạm mặt đất: mất telemetry và bản đồ cho tới khi mở lại."
                                                      + (telemetry.armed ? "

⚠ Drone đang ARM — chỉ thoát khi thật cần." : ""),
                                                      "THOÁT", Theme.danger, () => Qt.quit()) }
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

    // ── gán nguồn cho một chức năng (CH1..8) bằng cách gạt ─────────────────
    // Chọn chức năng → gạt cần / bật công tắc → nguồn dịch khỏi mốc (lúc mở)
    // nhiều nhất được phát hiện → GÁN. Chạm một nguồn để chọn tay (vd SD luôn
    // HIGH, không gạt được). Nguồn đã cấp kênh khác ⇒ hoán đổi (setChannelSource).
    Popup {
        id: learnPicker
        property int ch: 0
        property var base: []
        property int manual: -1   // nguồn chạm chọn tay, -1 = theo phát hiện
        property int locked: -1   // nguồn phát hiện gần nhất (giữ lại khi cần đã về giữa)
        readonly property int pick: manual >= 0 ? manual : locked
        readonly property int pickUsedBy: pick >= 0 ? rc.channelSources.indexOf(rc.controlNames[pick]) : -1
        function openFor(c) {
            ch = c; manual = -1; locked = -1
            base = [0, 1, 2, 3, 4, 5, 6, 7].map(i => rc.controls[i])
            open()
        }
        // nguồn dịch khỏi mốc nhiều nhất (> 250 µs) — cần ga không tự về giữa nên
        // so với mốc chứ không với 1500; công tắc nhảy ≥ 500 µs khi đổi vị trí
        readonly property int moving: {
            if (!opened || base.length < 8)
                return -1
            let best = -1, dev = 250
            for (let i = 0; i < 8; ++i) {
                const d = Math.abs(rc.controls[i] - base[i])
                if (d > dev) { dev = d; best = i }
            }
            return best
        }
        onMovingChanged: if (moving >= 0) locked = moving

        parent: root
        anchors.centerIn: parent
        width: Math.min(root.width - 40, 680)
        modal: true
        padding: 16
        background: Rectangle { radius: 10; color: "#fa0e1210"; border.color: Theme.stroke }

        contentItem: ColumnLayout {
            spacing: 10
            RowLayout {
                Layout.fillWidth: true
                Text { text: "GÁN " + root.chFuncs[learnPicker.ch] + "  (CH" + (learnPicker.ch + 1) + ")"
                       color: Theme.text; font.family: Theme.mono; font.pixelSize: 15; font.bold: true }
                Item { Layout.fillWidth: true }
                Text { text: "hiện tại: " + rc.channelSources[learnPicker.ch]
                       color: Theme.sub; font.family: Theme.mono; font.pixelSize: 11 }
            }
            // trạng thái phát hiện
            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 52
                radius: 8
                color: learnPicker.pick >= 0 ? "#1a26d07c" : Theme.panel
                border.color: learnPicker.pick >= 0 ? Theme.accent : Theme.strokeBtn
                Text {
                    anchors.centerIn: parent
                    text: learnPicker.pick >= 0
                          ? (learnPicker.manual >= 0 ? "ĐÃ CHỌN: " : "PHÁT HIỆN: ") + rc.controlNames[learnPicker.pick]
                          : "Gạt cần hoặc bật công tắc muốn dùng làm " + root.chFuncs[learnPicker.ch] + "…"
                    color: learnPicker.pick >= 0 ? Theme.accent : Theme.sub
                    font.family: learnPicker.pick >= 0 ? Theme.mono : Theme.ui
                    font.pixelSize: learnPicker.pick >= 0 ? 18 : 13
                    font.bold: learnPicker.pick >= 0
                }
            }
            // 8 nguồn, giá trị trực tiếp — chạm để chọn tay
            GridLayout {
                Layout.fillWidth: true
                columns: 4
                rowSpacing: 6; columnSpacing: 6
                Repeater {
                    model: 8
                    delegate: Rectangle {
                        id: srcCard
                        required property int index
                        readonly property int us: rc.controls[index]
                        readonly property bool picked: learnPicker.pick === index
                        readonly property bool live: learnPicker.moving === index
                        readonly property int usedBy: rc.channelSources.indexOf(rc.controlNames[index])
                        Layout.fillWidth: true
                        Layout.preferredHeight: 62
                        radius: 6
                        color: picked ? "#3326d07c" : Theme.panel
                        border.color: picked || live ? Theme.accent : Theme.strokeBtn
                        border.width: picked ? 2 : 1
                        ColumnLayout {
                            anchors.fill: parent
                            anchors.margins: 6
                            spacing: 3
                            RowLayout {
                                Layout.fillWidth: true
                                Text { text: rc.controlNames[srcCard.index]; color: Theme.text
                                       font.family: Theme.mono; font.pixelSize: 14; font.bold: true }
                                Item { Layout.fillWidth: true }
                                Text { text: srcCard.usedBy >= 0 ? "CH" + (srcCard.usedBy + 1) : "—"
                                       color: Theme.dim; font.family: Theme.mono; font.pixelSize: 10 }
                            }
                            Rectangle {
                                Layout.fillWidth: true
                                Layout.preferredHeight: 8
                                radius: 3
                                color: Theme.bg; border.color: Theme.strokeBtn
                                readonly property real f: Math.max(0, Math.min(1, (srcCard.us - 1000) / 1000))
                                Rectangle {
                                    y: 1; height: parent.height - 2; radius: 2
                                    x: Math.min(parent.width * 0.5, parent.width * parent.f)
                                    width: Math.abs(parent.width * parent.f - parent.width * 0.5)
                                    color: srcCard.live || srcCard.picked ? Theme.accent : Theme.sub
                                }
                                Rectangle { x: parent.width / 2 - 1; width: 2; height: parent.height; color: Theme.dim }
                            }
                            Text { text: rc.connected ? srcCard.us + " µs" : "—"
                                   color: Theme.sub; font.family: Theme.mono; font.pixelSize: 10 }
                        }
                        MouseArea { anchors.fill: parent; onClicked: learnPicker.manual = srcCard.index }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Text {
                    Layout.fillWidth: true
                    wrapMode: Text.WordWrap
                    text: learnPicker.pickUsedBy >= 0 && learnPicker.pickUsedBy !== learnPicker.ch
                          ? rc.controlNames[learnPicker.pick] + " đang ở CH" + (learnPicker.pickUsedBy + 1)
                            + " — CH đó sẽ nhận " + rc.channelSources[learnPicker.ch] + " (hoán đổi)."
                          : "Ghi RAM STM32, hiệu lực ngay. Kiểm tra hướng, đảo nếu cần, rồi LƯU."
                    color: Theme.dim; font.family: Theme.ui; font.pixelSize: 10
                }
                ActBtn { label: "LÀM LẠI"; tint: Theme.sub
                         onActivated: learnPicker.openFor(learnPicker.ch) }
                ActBtn { label: "HUỶ"; tint: Theme.sub; onActivated: learnPicker.close() }
                ActBtn { label: learnPicker.pick >= 0 ? "GÁN " + rc.controlNames[learnPicker.pick] : "GÁN"
                         tint: Theme.accent
                         enabled: learnPicker.pick >= 0 && !rc.busy
                         onActivated: { rc.setChannelSource(learnPicker.ch, learnPicker.pick); learnPicker.close() } }
            }
        }
    }

    // ── sửa giá trị giữ của một cần lái ─────────────────────────────────────
    Popup {
        id: holdEditor
        property int src: 0
        property int value: 1500
        function openFor(s, v) { src = s; value = v; open() }
        function bump(d) { value = Math.max(1000, Math.min(2000, value + d)) }

        parent: root
        anchors.centerIn: parent
        width: Math.min(root.width - 40, 440)
        modal: true
        padding: 16
        background: Rectangle { radius: 10; color: "#fa0e1210"; border.color: Theme.stroke }

        contentItem: ColumnLayout {
            spacing: 12
            Text {
                text: "Giá trị giữ " + root.stickNames[holdEditor.src] + " (" + root.channelOf(root.stickNames[holdEditor.src]) + ")"
                color: Theme.text; font.family: Theme.ui; font.pixelSize: 13; font.bold: true
            }
            Text {
                Layout.alignment: Qt.AlignHCenter
                text: holdEditor.value + " µs"
                color: Theme.accent; font.family: Theme.mono; font.pixelSize: 28; font.bold: true
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [-100, -10, 10, 100]
                    delegate: ActBtn { required property int modelData
                                       Layout.fillWidth: true
                                       label: (modelData > 0 ? "+" : "") + modelData; tint: Theme.text
                                       onActivated: holdEditor.bump(modelData) }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Repeater {
                    model: [1000, 1500, 2000]
                    delegate: ActBtn { required property int modelData
                                       Layout.fillWidth: true
                                       label: modelData; tint: Theme.sub
                                       onActivated: holdEditor.value = modelData }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                Item { Layout.fillWidth: true }
                ActBtn { label: "HUỶ"; tint: Theme.sub; onActivated: holdEditor.close() }
                ActBtn { label: "GHI"; tint: Theme.warn
                         onActivated: { rc.setStickHold(holdEditor.src, holdEditor.value); holdEditor.close() } }
            }
        }
    }

    // ── hộp xác nhận dùng chung ─────────────────────────────────────────────
    // ask(tiêu đề, nội dung, nhãn OK, màu, hàm OK[, nhãn phụ, hàm phụ])
    Popup {
        id: confirm
        property string title
        property string body
        property string okText
        property color okTint: Theme.accent
        property var okAction: null
        property string altText: ""
        property var altAction: null
        function ask(t, b, ok, tint, action, alt, altAct) {
            title = t; body = b; okText = ok; okTint = tint; okAction = action
            altText = alt || ""; altAction = altAct || null
            open()
        }

        parent: root
        anchors.centerIn: parent
        width: Math.min(root.width - 40, 520)
        modal: true
        padding: 16
        background: Rectangle { radius: 10; color: "#fa0e1210"; border.color: confirm.okTint }

        contentItem: ColumnLayout {
            spacing: 12
            Text { text: confirm.title; color: Theme.text
                   font.family: Theme.ui; font.pixelSize: 15; font.bold: true }
            Text { Layout.fillWidth: true; wrapMode: Text.WordWrap
                   text: confirm.body; color: Theme.sub
                   font.family: Theme.ui; font.pixelSize: 12 }
            RowLayout {
                Layout.fillWidth: true
                spacing: 6
                ActBtn { visible: confirm.altText !== ""; label: confirm.altText; tint: Theme.danger
                         onActivated: { confirm.close(); if (confirm.altAction) confirm.altAction() } }
                Item { Layout.fillWidth: true }
                ActBtn { label: "HUỶ"; tint: Theme.sub; onActivated: confirm.close() }
                ActBtn { label: confirm.okText; tint: confirm.okTint
                         onActivated: { confirm.close(); if (confirm.okAction) confirm.okAction() } }
            }
        }
    }

    // ── phần tử phụ ─────────────────────────────────────────────────────────

    // LƯU flash: luôn hỏi xác nhận, khoá khi drone đang ARM (§6.1)
    component SaveBtn: ActBtn {
        label: telemetry.armed ? "LƯU (ĐANG ARM)" : (rc.dirty ? "● LƯU" : "LƯU")
        tint: Theme.danger
        enabled: rc.connected && rc.dirty && !rc.busy && !rc.calActive && !telemetry.armed
        onActivated: confirm.ask("Lưu cấu hình xuống flash STM32?",
                                 "STM32 xoá và ghi lại một sector flash: luồng RC và radio dừng thoáng chốc. "
                                 + "Chỉ lưu khi drone đang ở dưới đất.",
                                 "LƯU", Theme.danger, () => rc.saveConfig())
    }

    component Card: Rectangle {
        default property alias content: cardCol.data
        property string title
        radius: 8
        color: Theme.panelSolid
        border.color: Theme.stroke
        ColumnLayout {
            id: cardCol
            anchors.fill: parent
            anchors.margins: 10
            spacing: 6
            Text { text: title; color: Theme.dim
                   font.family: Theme.ui; font.pixelSize: 10; font.bold: true; font.letterSpacing: 1.4 }
        }
    }

    component InfoRow: RowLayout {
        default property alias extra: extraRow.data
        property string label
        property string value
        property color valueColor: Theme.text
        Layout.fillWidth: true
        Layout.preferredHeight: 32
        spacing: 8
        Text { text: label; color: Theme.sub; font.family: Theme.ui; font.pixelSize: 11; Layout.fillWidth: true }
        Text { text: value; color: valueColor; font.family: Theme.mono; font.pixelSize: 12; font.bold: true }
        Row { id: extraRow }
    }

    // một hàng bảng hiệu chỉnh: 8 ô; ô 6 là thanh hành trình (span / calMinSpan)
    component CalRow: Rectangle {
        property var cells: []
        property bool header: false
        property bool selected: false
        property bool dimmed: false
        property int span: -1
        signal tapped()
        readonly property var widths: [24, 78, 40, 40, 40, 40, 96, 0] // 0 = phần còn lại
        Layout.fillWidth: true
        Layout.preferredHeight: header ? 18 : 31
        radius: 5
        color: selected && !header ? "#1637e0a0" : "transparent"
        border.color: selected && !header ? Theme.accent : (header ? "transparent" : Theme.strokeSoft)
        opacity: dimmed ? 0.45 : 1
        RowLayout {
            anchors.fill: parent
            anchors.leftMargin: 6; anchors.rightMargin: 6
            spacing: 6
            Repeater {
                model: cells.length
                delegate: Item {
                    required property int index
                    Layout.preferredWidth: widths[index]
                    Layout.fillWidth: widths[index] === 0
                    Layout.fillHeight: true
                    Text {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        elide: Text.ElideRight
                        visible: header || index !== 6
                        text: cells[index]
                        font.family: Theme.mono
                        font.pixelSize: header ? 9 : (index === 0 ? 15 : 12)
                        font.letterSpacing: header ? 1 : 0
                        font.bold: !header && index === 1
                        color: header ? Theme.dim
                               : index === 0 ? Theme.accent
                               : index === 1 ? Theme.text
                               : index === 2 ? Theme.accent
                               : index === 7 ? Theme.sub
                               : (rc.calActive ? Theme.warn : Theme.dim)
                    }
                    // thanh hành trình: xanh khi đạt ≥ calMinSpan
                    Rectangle {
                        visible: !header && index === 6 && span >= 0
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width; height: 14; radius: 3
                        color: Theme.panel; border.color: Theme.strokeBtn
                        Rectangle {
                            x: 1; y: 1; height: parent.height - 2; radius: 2
                            width: Math.max(0, (parent.width - 2) * Math.min(1, span / rc.calMinSpan))
                            color: span >= rc.calMinSpan ? Theme.accent : Theme.warn
                        }
                        Text { anchors.centerIn: parent; text: span; color: Theme.text
                               font.family: Theme.mono; font.pixelSize: 9 }
                    }
                }
            }
        }
        MouseArea { anchors.fill: parent; enabled: !header && !rc.calActive; onClicked: parent.tapped() }
    }

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
        implicitWidth: Math.max(70, abTxt.implicitWidth + 22); implicitHeight: 36; radius: 6
        color: Qt.rgba(tint.r, tint.g, tint.b, enabled ? 0.16 : 0.05)
        border.color: enabled ? tint : Theme.strokeBtn
        Text { id: abTxt; anchors.centerIn: parent; text: label
               color: parent.enabled ? parent.tint : Theme.dim
               font.family: Theme.mono; font.pixelSize: 12; font.bold: true }
        MouseArea { anchors.fill: parent; enabled: parent.enabled; onClicked: parent.activated() }
    }

    component Gimbal: ColumnLayout {
        id: gimbal
        property string title
        property int hCh: 3   // kênh trục ngang (chỉ số 0..3 = ROLL/PITCH/THR/YAW)
        property int vCh: 2   // kênh trục dọc
        readonly property int h: root.valOfCh(hCh)
        readonly property int v: root.valOfCh(vCh)
        // PITCH: đẩy cần lên cho PWM thấp (ArduPilot) ⇒ vẽ ngược để chấm đi theo tay
        readonly property int vSign: vCh === 1 ? -1 : 1
        spacing: 4
        Text { text: title; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1 }
        Rectangle {
            id: pad
            Layout.preferredWidth: 150; Layout.preferredHeight: 150
            radius: 8; color: Theme.panel; border.color: Theme.strokeBtn
            readonly property real nx: Math.max(-1, Math.min(1, (gimbal.h - 1500) / 500))
            readonly property real ny: gimbal.vSign * Math.max(-1, Math.min(1, (gimbal.v - 1500) / 500))
            Rectangle { x: parent.width / 2; y: 6; width: 1; height: parent.height - 12; color: Theme.stroke }
            Rectangle { y: parent.height / 2; x: 6; height: 1; width: parent.width - 12; color: Theme.stroke }
            Text { anchors.horizontalCenter: parent.horizontalCenter; y: 4
                   text: root.funcShort[gimbal.vCh]; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 9 }
            Text { anchors.verticalCenter: parent.verticalCenter; anchors.right: parent.right; anchors.rightMargin: 4
                   text: root.funcShort[gimbal.hCh]; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 9 }
            Rectangle {
                width: 16; height: 16; radius: 8
                color: rc.connected ? Theme.danger : Theme.dim
                x: parent.width / 2 + pad.nx * (parent.width / 2 - 10) - width / 2
                y: parent.height / 2 - pad.ny * (parent.height / 2 - 10) - height / 2
            }
        }
        Text { text: root.funcShort[gimbal.hCh] + "·" + root.srcOfCh(gimbal.hCh) + " " + (rc.connected ? gimbal.h : "—")
               color: Theme.text; font.family: Theme.mono; font.pixelSize: 11 }
        Text { text: root.funcShort[gimbal.vCh] + "·" + root.srcOfCh(gimbal.vCh) + " " + (rc.connected ? gimbal.v : "—")
               color: Theme.text; font.family: Theme.mono; font.pixelSize: 11 }
    }
}
