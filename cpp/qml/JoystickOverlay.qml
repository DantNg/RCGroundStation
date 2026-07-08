// Lớp phủ cần lái ảo trên vùng xem chính — hai cần lái ở hai góc dưới.
//
// Cần TRÁI:  dọc = ga (lên = tăng),  ngang = yaw (xoay mũi).
// Cần PHẢI:  dọc = pitch (tiến/lùi), ngang = roll (nghiêng trái/phải).
// Chỉ hiện khi `joystick.enabled`. Mỗi lần núm di chuyển gộp cả bốn trục rồi gọi
// joystick.setAxes(roll, pitch, yaw, throttle01). Ga chuẩn hoá 0..1 (giữa = 0.5).
import QtQuick
import GroundCtrl

Item {
    id: root
    visible: joystick.enabled

    // giá trị trục hiện tại (gộp từ hai cần)
    property real _roll: 0
    property real _pitch: 0
    property real _yaw: 0
    property real _throttle: 0.5

    function _push() {
        joystick.setAxes(_roll, _pitch, _yaw, _throttle);
    }

    // ── dải cảnh báo an toàn (trên cùng lớp phủ) ────────────────────────────
    Rectangle {
        anchors.top: parent.top
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.topMargin: 8
        width: warnRow.width + 24; height: 26; radius: 13
        color: "#cc1a0f0f"; border.color: Theme.warn
        Row {
            id: warnRow
            anchors.centerIn: parent; spacing: 8
            Rectangle { width: 8; height: 8; radius: 4; color: Theme.warn; anchors.verticalCenter: parent.verticalCenter }
            Text {
                text: "CẦN LÁI ẢO ĐANG BẬT — cần chế độ tay (STABILIZE/ALT_HOLD) & đã ARM"
                color: Theme.warn; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 0.3
                anchors.verticalCenter: parent.verticalCenter
            }
        }
    }

    // ── cần TRÁI: ga (dọc) + yaw (ngang) ────────────────────────────────────
    JoystickPad {
        id: leftPad
        width: 132; height: 132
        anchors.left: parent.left; anchors.bottom: parent.bottom
        anchors.leftMargin: 14; anchors.bottomMargin: 14
        label: "GA · YAW"; axisV: "GA"; axisH: "YAW"
        onMoved: (nx, ny) => {
            root._yaw = nx;
            root._throttle = (ny + 1) / 2; // -1..1 → 0..1 (giữa = 0.5)
            root._push();
        }
    }

    // ── cần PHẢI: pitch (dọc) + roll (ngang) ────────────────────────────────
    JoystickPad {
        id: rightPad
        width: 132; height: 132
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.rightMargin: 14; anchors.bottomMargin: 14
        label: "PITCH · ROLL"; axisV: "PITCH"; axisH: "ROLL"
        onMoved: (nx, ny) => {
            root._roll = nx;
            root._pitch = ny;
            root._push();
        }
    }
}
