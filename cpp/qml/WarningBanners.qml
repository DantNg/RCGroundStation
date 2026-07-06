// Banner cảnh báo trên-giữa. Nghe telemetry.noticeReceived; STATUSTEXT mức
// Warning (4) trở lên nặng hiện thành dải, tự tắt sau 6 s (một Timer prune chung).
import QtQuick
import GroundCtrl

Column {
    id: root
    spacing: 5

    ListModel { id: banners }

    Connections {
        target: telemetry
        function onNoticeReceived(severity, text) {
            if (severity <= 4) // Warning hoặc nặng hơn
                banners.insert(0, { severity: severity, text: text, born: Date.now() });
        }
    }

    // dọn dải quá 6 s (duyệt từ cuối để xoá an toàn)
    Timer {
        interval: 1000; running: true; repeat: true
        onTriggered: {
            const now = Date.now();
            for (let i = banners.count - 1; i >= 0; i--)
                if (now - banners.get(i).born > 6000)
                    banners.remove(i);
        }
    }

    Repeater {
        model: banners
        delegate: Rectangle {
            id: bn
            required property int severity
            required property string text
            width: Math.min(root.width, 560)
            anchors.horizontalCenter: parent.horizontalCenter
            height: 30; radius: 6
            color: "#e6160b0d"
            border.color: severity <= 3 ? Theme.danger : Theme.warn

            Row {
                anchors.left: parent.left; anchors.leftMargin: 10
                anchors.verticalCenter: parent.verticalCenter
                spacing: 8
                Rectangle { width: 8; height: 8; radius: 4
                            color: bn.severity <= 3 ? Theme.danger : Theme.warn
                            anchors.verticalCenter: parent.verticalCenter }
                Text { text: bn.text; color: Theme.text
                       font.family: Theme.mono; font.pixelSize: 12
                       anchors.verticalCenter: parent.verticalCenter }
            }
        }
    }
}
