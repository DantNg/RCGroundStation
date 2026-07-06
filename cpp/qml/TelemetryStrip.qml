// Dải telemetry dưới đáy (80px) — 7 ô: GND SPD · ALT REL · DIST HOME · HEADING ·
// GPS · RF LINK · BATTERY. Mỗi ô: nhãn + giá trị+đơn vị + dòng phụ. Bám mockup.
import QtQuick
import QtQuick.Layouts
import QtPositioning
import GroundCtrl

Rectangle {
    id: root
    color: Theme.panelSolid

    // viền trên
    Rectangle { anchors.top: parent.top; width: parent.width; height: 1; color: Theme.strokeSoft }

    // HOME = fix hợp lệ đầu tiên → tính khoảng cách về nhà
    property var homeCoord: null
    Connections {
        target: telemetry
        function onPositionChanged() {
            if (telemetry.posValid && root.homeCoord === null)
                root.homeCoord = QtPositioning.coordinate(telemetry.lat, telemetry.lon);
        }
    }
    readonly property int distHome: (homeCoord !== null && telemetry.posValid)
        ? Math.round(homeCoord.distanceTo(QtPositioning.coordinate(telemetry.lat, telemetry.lon)))
        : 0

    function cardinal(h) {
        const dirs = ["N","NE","E","SE","S","SW","W","NW"];
        return dirs[Math.round((((h % 360) + 360) % 360) / 45) % 8];
    }

    RowLayout {
        anchors.fill: parent
        anchors.topMargin: 1
        spacing: 0

        Tile { label: "GND SPD"; value: telemetry.groundspeed.toFixed(1); unit: "m/s"
               sub: "AIR " + telemetry.airspeed.toFixed(1) }
        Tile { label: "ALT REL"; value: telemetry.altRel.toFixed(1); unit: "m"
               sub: "ABS " + telemetry.altMsl.toFixed(0) + "m" }
        Tile { label: "DIST HOME"; value: String(root.distHome); unit: "m"; sub: "RTL READY" }
        Tile { label: "HEADING"; value: Math.round(telemetry.heading) + "°"; unit: ""
               sub: root.cardinal(telemetry.heading) }
        Tile { label: "GPS"; value: String(telemetry.satellites); unit: "sat"
               sub: telemetry.fixLabel; valueColor: Theme.accent }
        Tile { label: "RF LINK"; value: telemetry.linkPct + "%"; unit: ""
               sub: telemetry.linkPct > 75 ? "STRONG" : (telemetry.linkPct > 45 ? "NOMINAL" : "WEAK")
               valueColor: telemetry.linkPct > 45 ? Theme.text : Theme.warn }
        Tile { label: "BATTERY"; last: true
               value: (telemetry.battPct < 0 ? "—" : telemetry.battPct + "%"); unit: ""
               sub: telemetry.battVolt.toFixed(1) + "V · " + telemetry.battCurrent.toFixed(1) + "A"
               valueColor: telemetry.battPct > 50 ? Theme.accent
                           : (telemetry.battPct > 22 ? Theme.warn : Theme.danger) }
    }

    component Tile: Item {
        property string label
        property string value
        property string unit
        property string sub
        property color valueColor: Theme.text
        property bool last: false
        Layout.fillWidth: true
        Layout.fillHeight: true

        // viền phải giữa các ô
        Rectangle { visible: !parent.last; anchors.right: parent.right; width: 1; height: parent.height; color: "#161c19" }

        Column {
            anchors.left: parent.left; anchors.leftMargin: 11
            anchors.verticalCenter: parent.verticalCenter
            spacing: 4
            Text { text: label; color: Theme.dim; font.family: Theme.ui; font.pixelSize: 8; font.bold: true; font.letterSpacing: 1.4 }
            Row {
                spacing: 3
                Text { text: value; color: valueColor; font.family: Theme.mono; font.pixelSize: 20; font.bold: true }
                Text { anchors.bottom: parent.bottom; anchors.bottomMargin: 2
                       text: unit; color: Theme.sub; font.family: Theme.mono; font.pixelSize: 9; visible: unit !== "" }
            }
            Text { text: sub; color: Theme.sub; font.family: Theme.mono; font.pixelSize: 8; font.letterSpacing: 0.3 }
        }
    }
}
