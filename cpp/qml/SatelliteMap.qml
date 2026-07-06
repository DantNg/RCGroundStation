// Bản đồ nền 2D VỆ TINH — Qt Location + ảnh Esri World Imagery.
//
// Ảnh vệ tinh Esri dùng thứ tự tile z/y/x, mà custom.host của plugin OSM chỉ ghép
// được z/x/y.png; nên ta nạp qua "providers repository" cục bộ: main.cpp trỏ
// `mapProvidersUrl` tới thư mục providers/ (cạnh binary) chứa file "satellite"
// (Esri) + "street" (OSM). Khi tải xong, chọn map type SatelliteMapDay.
//
// Overlay chiến thuật giữ nguyên tinh thần mockup: la bàn, toạ độ, ký hiệu drone
// xoay theo hướng, dấu HOME, và tap-để-GOTO → backend.flyTo() (lệnh GUIDED thật).
import QtQuick
import QtLocation
import QtPositioning
import GroundCtrl

Item {
    id: root
    property var gotoCoord: null
    property var homeCoord: null
    readonly property bool hasGoto: gotoCoord !== null
    // Bám phương tiện khi true; kéo bản đồ sẽ tắt bám (để xem tự do) tới khi bấm
    // nút định tâm lại.
    property bool follow: true

    Plugin {
        id: mapPlugin
        name: "osm"
        PluginParameter { name: "osm.mapping.providersrepository.address"; value: mapProvidersUrl }
        PluginParameter { name: "osm.mapping.highdpi_tiles"; value: "false" }
        PluginParameter { name: "osm.useragent"; value: "LiteGCS-QML/1.0 (ground station)" }
    }

    // ghi HOME ở fix hợp lệ đầu tiên + bám phương tiện khi đang theo dõi
    Connections {
        target: telemetry
        function onPositionChanged() {
            if (telemetry.posValid && root.homeCoord === null)
                root.homeCoord = QtPositioning.coordinate(telemetry.lat, telemetry.lon);
            if (root.follow && telemetry.posValid)
                map.center = QtPositioning.coordinate(telemetry.lat, telemetry.lon);
        }
    }

    Map {
        id: map
        anchors.fill: parent
        plugin: mapPlugin
        copyrightsVisible: false
        // Tâm KHÔNG bind cứng vào telemetry (sẽ chống lại thao tác kéo). Đặt tâm
        // ban đầu; sau đó Connections ở trên tự bám khi follow=true, còn kéo tay
        // thì tắt follow.
        // Zoom tối đa 19 = trần cứng của plugin OSM (Qt Location). Bắt đầu ở 18
        // cho vùng bay hẹp.
        zoomLevel: 18
        maximumZoomLevel: 19

        // kéo để di chuyển bản đồ (tự tắt bám phương tiện)
        DragHandler {
            target: null
            property real lastX: 0
            property real lastY: 0
            onActiveChanged: {
                if (active) { lastX = 0; lastY = 0; root.follow = false; }
            }
            onTranslationChanged: (t) => {
                map.pan(lastX - translation.x, lastY - translation.y);
                lastX = translation.x;
                lastY = translation.y;
            }
        }

        // chụm 2 ngón để phóng to/thu nhỏ (màn cảm ứng cầm tay) — trước đây chỉ
        // có con lăn chuột nên trên cảm ứng không zoom được tới giới hạn.
        PinchHandler {
            id: pinch
            target: null
            property real zoomStart: 0
            onActiveChanged: if (active) zoomStart = map.zoomLevel
            onActiveScaleChanged: {
                map.zoomLevel = Math.max(map.minimumZoomLevel,
                    Math.min(map.maximumZoomLevel, zoomStart + Math.log2(activeScale)));
            }
        }

        // chọn lớp vệ tinh khi kho nhà cung cấp đã tải xong
        function selectSatellite() {
            for (let i = 0; i < supportedMapTypes.length; i++) {
                if (supportedMapTypes[i].style === MapType.SatelliteMapDay) {
                    activeMapType = supportedMapTypes[i];
                    return;
                }
            }
        }
        Component.onCompleted: {
            center = telemetry.posValid
                ? QtPositioning.coordinate(telemetry.lat, telemetry.lon)
                : QtPositioning.coordinate(21.0278, 105.8342); // Hà Nội (mặc định)
            selectSatellite();
        }
        onSupportedMapTypesChanged: selectSatellite()

        WheelHandler {
            onWheel: (ev) => map.zoomLevel = Math.max(map.minimumZoomLevel,
                        Math.min(map.maximumZoomLevel, map.zoomLevel + (ev.angleDelta.y > 0 ? 0.5 : -0.5)))
        }

        // ── dấu HOME (kim cương lục) ──────────────────────────────────────
        MapQuickItem {
            visible: root.homeCoord !== null
            coordinate: root.homeCoord || QtPositioning.coordinate()
            anchorPoint.x: 8; anchorPoint.y: 8
            sourceItem: Item {
                width: 16; height: 26
                Rectangle {
                    width: 16; height: 16; rotation: 45
                    color: "#2937e0a0"; border.color: Theme.accent; border.width: 1.5
                }
                Text { text: "HOME"; color: Theme.accent; font.family: Theme.mono; font.pixelSize: 8; font.letterSpacing: 1
                       anchors.horizontalCenter: parent.horizontalCenter; y: 16 }
            }
        }

        // ── ký hiệu drone (chevron xoay theo hướng) ───────────────────────
        MapQuickItem {
            visible: telemetry.posValid
            coordinate: QtPositioning.coordinate(telemetry.lat, telemetry.lon)
            anchorPoint.x: 14; anchorPoint.y: 14
            sourceItem: Item {
                width: 28; height: 28
                rotation: telemetry.heading
                Canvas {
                    id: chevron
                    anchors.fill: parent
                    property color fill: telemetry.armed ? Theme.danger : Theme.accent
                    onFillChanged: requestPaint()
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.clearRect(0, 0, width, height);
                        ctx.beginPath();
                        ctx.arc(width / 2, height / 2, 12, 0, Math.PI * 2);
                        ctx.fillStyle = "rgba(255,59,59,0.10)";
                        ctx.fill();
                        ctx.beginPath();
                        ctx.moveTo(width / 2, 2);
                        ctx.lineTo(width - 5, height - 4);
                        ctx.lineTo(width / 2, height * 0.72);
                        ctx.lineTo(5, height - 4);
                        ctx.closePath();
                        ctx.fillStyle = fill; ctx.fill();
                        ctx.strokeStyle = "#ffffff"; ctx.lineWidth = 1.2; ctx.stroke();
                    }
                    Component.onCompleted: requestPaint()
                }
            }
        }

        // ── dấu GOTO ──────────────────────────────────────────────────────
        MapQuickItem {
            visible: root.hasGoto
            coordinate: root.gotoCoord || QtPositioning.coordinate()
            anchorPoint.x: 8; anchorPoint.y: 8
            sourceItem: Item {
                width: 16; height: 16
                Rectangle { anchors.centerIn: parent; width: 16; height: 16; rotation: 45
                            color: "transparent"; border.color: Theme.danger; border.width: 1.5 }
            }
        }

        TapHandler {
            onTapped: (ep) => root.gotoCoord = map.toCoordinate(ep.position)
        }
    }

    // ── nút định tâm lại (hiện khi đã kéo rời khỏi phương tiện) ──────────────
    Rectangle {
        visible: !root.follow
        anchors.right: parent.right; anchors.bottom: parent.bottom
        anchors.rightMargin: 8; anchors.bottomMargin: 8
        width: rcRow.implicitWidth + 16; height: 26; radius: 6
        color: "#cc0a0d0b"; border.color: Theme.accent
        Row {
            id: rcRow
            anchors.centerIn: parent; spacing: 5
            Text { text: "◎"; color: Theme.accent; font.pixelSize: 13; anchors.verticalCenter: parent.verticalCenter }
            Text { text: "ĐỊNH TÂM"; color: Theme.accent; font.family: Theme.mono; font.pixelSize: 10; font.bold: true
                   anchors.verticalCenter: parent.verticalCenter }
        }
        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
            onClicked: {
                root.follow = true;
                if (telemetry.posValid)
                    map.center = QtPositioning.coordinate(telemetry.lat, telemetry.lon);
            }
        }
    }

    // ── la bàn (trên-giữa) ──────────────────────────────────────────────────
    Item {
        width: 56; height: 56
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top; anchors.topMargin: 8
        Rectangle { anchors.fill: parent; radius: 28; color: "#996060a09"; border.color: "#24ffffff" }
        Item {
            anchors.fill: parent
            rotation: telemetry.heading
            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter; y: 4
                width: 0; height: 0
                Canvas {
                    width: 10; height: 12; x: -5
                    onPaint: {
                        const ctx = getContext("2d");
                        ctx.beginPath(); ctx.moveTo(5, 12); ctx.lineTo(0, 0); ctx.lineTo(10, 0); ctx.closePath();
                        ctx.fillStyle = Theme.danger; ctx.fill();
                    }
                }
            }
        }
        Text { text: "N"; color: Theme.text; font.family: Theme.mono; font.pixelSize: 8
               anchors.horizontalCenter: parent.horizontalCenter; y: -2 }
        Text { anchors.centerIn: parent; text: Math.round(telemetry.heading) + "°"
               color: Theme.text; font.family: Theme.mono; font.pixelSize: 11 }
    }

    // ── nhắc TAP (trên-trái) ────────────────────────────────────────────────
    Rectangle {
        x: 8; y: 8; radius: 3; color: "#8c060a09"
        width: tapTxt.implicitWidth + 10; height: tapTxt.implicitHeight + 4
        Text { id: tapTxt; anchors.centerIn: parent; text: "◈ TAP MAP → SET GOTO"
               color: Theme.danger; font.family: Theme.mono; font.pixelSize: 8; font.letterSpacing: 0.6 }
    }

    // ── toạ độ hiện tại (dưới-trái) ─────────────────────────────────────────
    Text {
        x: 8; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        text: telemetry.posValid
              ? telemetry.lat.toFixed(4) + "° · " + telemetry.lon.toFixed(4) + "° · ZOOM " + Math.round(map.zoomLevel)
              : "— NO FIX — ZOOM " + Math.round(map.zoomLevel)
        color: telemetry.posValid ? "#6f7a76" : Theme.dim
        font.family: Theme.mono; font.pixelSize: 9; font.letterSpacing: 0.5
        style: Text.Outline; styleColor: "#a0000000"
    }

    // ── panel xác nhận GOTO (dưới-giữa) ─────────────────────────────────────
    Rectangle {
        visible: root.hasGoto
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: parent.bottom; anchors.bottomMargin: 10
        width: goRow.implicitWidth + 18; height: 34; radius: 9
        color: "#f00a0d0b"; border.color: "#80ff3b3b"

        Row {
            id: goRow
            anchors.centerIn: parent; spacing: 8
            Text { anchors.verticalCenter: parent.verticalCenter
                   text: "GOTO SET · FLY TO POINT?"; color: Theme.text
                   font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 0.4 }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: flyTxt.implicitWidth + 20; height: 22; radius: 5
                color: telemetry.connected ? "#d9ff3b3b" : "transparent"
                border.color: Theme.danger
                opacity: telemetry.connected ? 1 : 0.5
                Text { id: flyTxt; anchors.centerIn: parent; text: "FLY HERE"
                       color: telemetry.connected ? "#1a0505" : Theme.danger
                       font.family: Theme.mono; font.pixelSize: 10; font.bold: true; font.letterSpacing: 0.6 }
                MouseArea {
                    anchors.fill: parent
                    enabled: telemetry.connected && root.hasGoto
                    cursorShape: Qt.PointingHandCursor
                    onClicked: {
                        const alt = telemetry.altRel > 1 ? telemetry.altRel : 30;
                        backend.flyTo(root.gotoCoord.latitude, root.gotoCoord.longitude, alt);
                        root.gotoCoord = null;
                    }
                }
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: cxTxt.implicitWidth + 16; height: 22; radius: 5
                color: "transparent"; border.color: "#3a3340"
                Text { id: cxTxt; anchors.centerIn: parent; text: "CANCEL"
                       color: "#b9b7c2"; font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 0.6 }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: root.gotoCoord = null }
            }
        }
    }
}
