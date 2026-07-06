// Vùng xem CAM — webcam thật (Qt Multimedia) làm "downlink FPV", phủ overlay
// theo mockup: reticle 4 góc, REC + đồng hồ, badge SPD/ALT hai bên, hướng dưới.
// Feed RTSP/H.265 thật sau này chỉ cần thay Camera bằng MediaPlayer(source: url).
import QtQuick
import QtMultimedia
import GroundCtrl

Item {
    id: root
    property bool active: true
    clip: true

    MediaDevices { id: devices }
    readonly property bool hasCamera: devices.videoInputs.length > 0

    // nền sọc khi không có feed
    Rectangle {
        anchors.fill: parent
        gradient: Gradient {
            orientation: Gradient.Horizontal
            GradientStop { position: 0.0; color: "#0c100e" }
            GradientStop { position: 0.5; color: "#0a0d0b" }
            GradientStop { position: 1.0; color: "#0c100e" }
        }
    }

    CaptureSession {
        camera: Camera {
            id: cam
            cameraDevice: devices.defaultVideoInput
            active: root.active && root.hasCamera
        }
        videoOutput: view
    }
    VideoOutput {
        id: view
        anchors.fill: parent
        fillMode: VideoOutput.PreserveAspectCrop
        visible: root.hasCamera && root.active
    }

    // placeholder khi chưa có camera
    Column {
        anchors.centerIn: parent
        spacing: 6
        visible: !view.visible
        Text { text: "◉ TÍN HIỆU CAMERA"; color: "#5c6560"; font.family: Theme.mono; font.pixelSize: 12; font.letterSpacing: 4
               anchors.horizontalCenter: parent.horizontalCenter }
        Text { text: "KHÔNG CÓ TÍN HIỆU · H.265 · 30 FPS · GIMBAL −12°"; color: "#454d49"; font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 1
               anchors.horizontalCenter: parent.horizontalCenter }
    }

    // ── reticle giữa ────────────────────────────────────────────────────────
    Item {
        anchors.centerIn: parent
        width: 80; height: 80
        property color rc: "#d9ff3b3b"
        Repeater {
            model: [ [0,0,1,1], [1,0,-1,1], [0,1,1,-1], [1,1,-1,-1] ]
            delegate: Item {
                required property var modelData
                width: 16; height: 16
                x: modelData[0] * (parent.width - 16)
                y: modelData[1] * (parent.height - 16)
                Rectangle { width: 16; height: 2; color: parent.parent.rc
                            y: modelData[3] > 0 ? 0 : parent.height - 2 }
                Rectangle { width: 2; height: 16; color: parent.parent.rc
                            x: modelData[2] > 0 ? 0 : parent.width - 2 }
            }
        }
        Rectangle { anchors.centerIn: parent; width: 5; height: 5; radius: 2.5; color: Theme.danger }
    }

    // ── REC + đồng hồ (trên-trái) ───────────────────────────────────────────
    Row {
        x: 10; y: 10; spacing: 6
        Rectangle {
            width: 8; height: 8; radius: 4; color: "#ff4d4d"; anchors.verticalCenter: parent.verticalCenter
            SequentialAnimation on opacity {
                loops: Animation.Infinite
                NumberAnimation { to: 0.2; duration: 550 }
                NumberAnimation { to: 1.0; duration: 550 }
            }
        }
        Text { text: "REC " + telemetry.armElapsed; color: "#ff6b6b"
               font.family: Theme.mono; font.pixelSize: 10; anchors.verticalCenter: parent.verticalCenter }
    }

    // ── badge SPD (trái) ────────────────────────────────────────────────────
    SideBadge {
        anchors.left: parent.left; anchors.leftMargin: 10; anchors.verticalCenter: parent.verticalCenter
        caption: "TỐC ĐỘ"; value: telemetry.groundspeed.toFixed(1); valueColor: Theme.accent
    }
    // ── badge ALT (phải) ────────────────────────────────────────────────────
    SideBadge {
        anchors.right: parent.right; anchors.rightMargin: 10; anchors.verticalCenter: parent.verticalCenter
        caption: "CAO ĐỘ"; value: telemetry.altRel.toFixed(0); valueColor: Theme.text
    }

    // ── hướng (dưới-giữa) ───────────────────────────────────────────────────
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter; anchors.bottom: parent.bottom; anchors.bottomMargin: 10
        width: hdgTxt.implicitWidth + 24; height: 20; radius: 4
        color: "#b8060c0a"; border.color: "#24ffffff"
        Text {
            id: hdgTxt; anchors.centerIn: parent
            text: Math.round(telemetry.heading) + "° " + root.cardinal(telemetry.heading)
            color: Theme.text; font.family: Theme.mono; font.pixelSize: 11
        }
    }

    function cardinal(h) {
        const dirs = ["N","NE","E","SE","S","SW","W","NW"];
        return dirs[Math.round((((h % 360) + 360) % 360) / 45) % 8];
    }

    component SideBadge: Rectangle {
        property string caption
        property string value
        property color valueColor: Theme.text
        width: 58; height: 44; radius: 5
        color: "#b8060c0a"; border.color: "#24ffffff"
        Column {
            anchors.centerIn: parent; spacing: 1
            Text { text: caption; color: "#8a938d"; font.family: Theme.mono; font.pixelSize: 8
                   anchors.horizontalCenter: parent.horizontalCenter }
            Text { text: value; color: valueColor; font.family: Theme.mono; font.pixelSize: 16; font.bold: true
                   anchors.horizontalCenter: parent.horizontalCenter }
        }
    }
}
