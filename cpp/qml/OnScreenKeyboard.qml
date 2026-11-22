// Bàn phím ảo cho màn cảm ứng (tay cầm không có bàn phím): chữ / số / ký hiệu,
// SHIFT, ⌫, cách. Gõ thẳng vào `target` (TextField/TextInput); phím OK phát
// accepted(). Bàn phím thật vẫn gõ được vào target như thường.
import QtQuick
import QtQuick.Layouts
import GroundCtrl

ColumnLayout {
    id: kb
    property Item target
    property bool shift: false
    property bool symbols: false
    property string okText: "OK"
    property bool okEnabled: true
    signal accepted()

    spacing: 5

    readonly property var letterRows: [
        "1234567890".split(""),
        "qwertyuiop".split(""),
        "asdfghjkl".split(""),
        "zxcvbnm".split("")
    ]
    readonly property var symbolRows: [
        "1234567890".split(""),
        "!@#$%^&*()".split(""),
        "-_=+[]{};:".split(""),
        "'\",.<>/?\\|~`".split("")
    ]
    readonly property var rows: symbols ? symbolRows : letterRows

    function type(ch) {
        if (!target) return
        target.insert(target.cursorPosition, shift && !symbols ? ch.toUpperCase() : ch)
        if (shift && !symbols) shift = false   // SHIFT một lần như điện thoại
    }
    function backspace() {
        if (!target || target.cursorPosition === 0) return
        target.remove(target.cursorPosition - 1, target.cursorPosition)
    }

    Repeater {
        model: kb.rows.length
        delegate: RowLayout {
            id: kbRow
            required property int index
            Layout.fillWidth: true
            spacing: 5

            // hàng cuối của bàn phím chữ: SHIFT … ⌫
            Key {
                visible: kbRow.index === 3
                label: kb.symbols ? "" : "⇧"
                Layout.preferredWidth: 58
                highlighted: kb.shift
                enabled: !kb.symbols
                onPressed: kb.shift = !kb.shift
            }
            Repeater {
                model: kb.rows[kbRow.index]
                delegate: Key {
                    required property string modelData
                    Layout.fillWidth: true
                    label: kb.shift && !kb.symbols ? modelData.toUpperCase() : modelData
                    onPressed: kb.type(modelData)
                }
            }
            Key {
                visible: kbRow.index === 3
                label: "⌫"
                Layout.preferredWidth: 58
                onPressed: kb.backspace()
            }
        }
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: 5
        Key { label: kb.symbols ? "ABC" : "?123"; Layout.preferredWidth: 72; onPressed: kb.symbols = !kb.symbols }
        Key { label: "CÁCH"; Layout.fillWidth: true; onPressed: kb.type(" ") }
        Key {
            label: kb.okText; Layout.preferredWidth: 150
            accent: true; enabled: kb.okEnabled
            onPressed: kb.accepted()
        }
    }

    component Key: Rectangle {
        id: key
        property string label
        property bool highlighted: false
        property bool accent: false
        signal pressed()
        Layout.preferredHeight: 40
        Layout.preferredWidth: 40
        radius: 6
        opacity: enabled ? 1 : 0.35
        color: ma.pressed ? (accent ? "#2fbf88" : "#26302a")
               : accent ? Theme.accent
               : highlighted ? "#1f3a2e" : Theme.panel
        border.color: accent ? Theme.accent : (highlighted ? Theme.accent : Theme.strokeBtn)
        Text {
            anchors.centerIn: parent
            text: key.label
            color: key.accent ? "#04140d" : Theme.text
            font.family: Theme.mono; font.pixelSize: key.label.length > 1 ? 12 : 16; font.bold: key.accent
        }
        MouseArea { id: ma; anchors.fill: parent; onClicked: key.pressed() }
    }
}
