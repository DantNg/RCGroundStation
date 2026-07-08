// Cần lái ảo tái sử dụng — đế tròn + núm kéo được, tự về giữa khi nhả tay.
//
// Phát tín hiệu moved(nx, ny) với nx, ny ∈ [-1, 1]: nx dương = phải, ny dương =
// LÊN (đã đảo trục màn hình). Núm bị kẹp trong bán kính đế. Nhả tay → núm về
// giữa và phát moved(0, 0). Dùng cho cả cần trái (ga/yaw) và phải (pitch/roll).
import QtQuick
import GroundCtrl

Item {
    id: root

    // nhãn hiển thị dưới đế
    property string label: ""
    // nhãn hai trục (để người dùng biết trục nào là gì)
    property string axisH: ""
    property string axisV: ""

    // vị trí chuẩn hoá hiện tại
    property real nx: 0
    property real ny: 0

    signal moved(real nx, real ny)

    implicitWidth: 140
    implicitHeight: 140

    readonly property real baseRadius: Math.min(width, height) / 2
    readonly property real knobRadius: baseRadius * 0.34
    readonly property real maxTravel: baseRadius - knobRadius

    // ── đế ──────────────────────────────────────────────────────────────────
    Rectangle {
        id: base
        anchors.centerIn: parent
        width: root.baseRadius * 2
        height: width
        radius: width / 2
        color: "#cc0a0d0b" // nền đế bán trong suốt
        border.color: ma.pressed ? Theme.accent : Theme.strokeBtn
        border.width: 1.5

        // vạch thập giữa
        Rectangle { anchors.centerIn: parent; width: parent.width * 0.7; height: 1; color: Theme.strokeSoft }
        Rectangle { anchors.centerIn: parent; width: 1; height: parent.height * 0.7; color: Theme.strokeSoft }

        // nhãn trục
        Text {
            visible: root.axisV.length > 0
            text: root.axisV; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 8
            anchors.top: parent.top; anchors.topMargin: 6; anchors.horizontalCenter: parent.horizontalCenter
        }
        Text {
            visible: root.axisH.length > 0
            text: root.axisH; color: Theme.dim; font.family: Theme.mono; font.pixelSize: 8
            anchors.right: parent.right; anchors.rightMargin: 6; anchors.verticalCenter: parent.verticalCenter
        }
    }

    // ── núm ─────────────────────────────────────────────────────────────────
    Rectangle {
        id: knob
        width: root.knobRadius * 2
        height: width
        radius: width / 2
        // vị trí tính từ tâm; y đảo dấu (ny dương = lên = y âm trên màn hình)
        x: root.width / 2 - width / 2 + root.nx * root.maxTravel
        y: root.height / 2 - height / 2 - root.ny * root.maxTravel
        color: ma.pressed ? Theme.accent : "#37e0a0"
        opacity: ma.pressed ? 1.0 : 0.85
        border.color: "#0a0d0b"; border.width: 2

        // hồi giữa mượt khi nhả tay
        Behavior on x { enabled: !ma.pressed; NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
        Behavior on y { enabled: !ma.pressed; NumberAnimation { duration: 90; easing.type: Easing.OutCubic } }
    }

    // nhãn dưới
    Text {
        visible: root.label.length > 0
        text: root.label; color: Theme.sub
        font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: base.bottom; anchors.topMargin: 2
    }

    MouseArea {
        id: ma
        anchors.fill: parent
        preventStealing: true

        function update(mx, my) {
            // vector từ tâm, kẹp trong maxTravel
            let dx = mx - root.width / 2;
            let dy = my - root.height / 2;
            const dist = Math.hypot(dx, dy);
            if (dist > root.maxTravel && dist > 0) {
                dx = dx / dist * root.maxTravel;
                dy = dy / dist * root.maxTravel;
            }
            root.nx = dx / root.maxTravel;
            root.ny = -dy / root.maxTravel; // đảo: lên = dương
            root.moved(root.nx, root.ny);
        }

        onPressed: (m) => update(m.x, m.y)
        onPositionChanged: (m) => { if (pressed) update(m.x, m.y); }
        onReleased: {
            root.nx = 0; root.ny = 0;
            root.moved(0, 0);
        }
        onCanceled: {
            root.nx = 0; root.ny = 0;
            root.moved(0, 0);
        }
    }
}
