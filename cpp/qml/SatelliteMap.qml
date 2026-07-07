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
        // Google trả 403 nếu User-Agent lạ → giả lập trình duyệt như Mission Planner.
        PluginParameter { name: "osm.useragent"; value: "Mozilla/5.0 (Windows NT 10.0; Win64; x64) AppleWebKit/537.36 (KHTML, like Gecko) Chrome/124.0 Safari/537.36" }
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
        // Trần zoom = MaximumZoomLevel trong providers/satellite (Esri có tile
        // tới ~z21-23 ở đô thị). Bắt đầu ở 18 cho vùng bay hẹp.
        zoomLevel: 18
        maximumZoomLevel: 22

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
        onErrorChanged: if (error !== Map.NoError)
            console.warn("Bản đồ lỗi:", error, errorString);

        WheelHandler {
            onWheel: (ev) => map.zoomLevel = Math.max(map.minimumZoomLevel,
                        Math.min(map.maximumZoomLevel, map.zoomLevel + (ev.angleDelta.y > 0 ? 0.5 : -0.5)))
        }

        // HOME / GOTO / WP KHÔNG vẽ bằng MapQuickItem nữa — xem các overlay bên
        // dưới (MapQuickItem bị ô chữ nhật đục trên Linux/Pi do FBO không xoá alpha).

        TapHandler {
            onTapped: (ep) => {
                root.gotoCoord = map.toCoordinate(ep.position);
                // chia sẻ điểm vừa chọn ra mạng cầu nối (no-op nếu cầu nối tắt)
                const alt = telemetry.altRel > 1 ? telemetry.altRel : 30;
                backend.shareWaypoint(root.gotoCoord.latitude, root.gotoCoord.longitude, alt);
            }
        }
    }

    // ── ký hiệu drone (OVERLAY, KHÔNG dùng MapQuickItem) ─────────────────────
    // Trên Linux/Pi, MapQuickItem "grab" sourceItem thành texture nền ĐỤC → che
    // map thành ô chữ nhật ngay tại drone. Ở đây vẽ như overlay QML thường và
    // chiếu toạ độ GPS ra pixel bằng map.fromCoordinate() → composite trong suốt
    // bình thường. Chevron dựng bằng Rectangle thuần (không cần Canvas/Shapes).
    Item {
        id: droneOverlay
        visible: telemetry.posValid
        width: 40; height: 40
        z: 42
        // chiếu toạ độ drone ra pixel; tham chiếu center/zoom/kích thước để tái
        // tính mỗi khi map pan/zoom (không chỉ khi vị trí drone đổi)
        property point scr: {
            void map.center; void map.zoomLevel; void map.width; void map.height;
            return telemetry.posValid
                ? map.fromCoordinate(QtPositioning.coordinate(telemetry.lat, telemetry.lon), false)
                : Qt.point(-10000, -10000);
        }
        x: scr.x - width / 2
        y: scr.y - height / 2

        // quầng mờ cố định
        Rectangle {
            anchors.centerIn: parent
            width: 26; height: 26; radius: 13
            color: "#1aff3b3b"
        }

        // chevron hướng (^) — hai thanh Rectangle xoay quanh tâm theo heading
        Item {
            anchors.fill: parent
            rotation: telemetry.heading
            transformOrigin: Item.Center
            property color fill: telemetry.armed ? Theme.danger : Theme.accent
            Rectangle {   // cánh trái "/"
                x: 13.5; y: 10; width: 5; height: 18; radius: 2.5
                color: parent.fill; antialiasing: true
                rotation: 26.6; transformOrigin: Item.Center
            }
            Rectangle {   // cánh phải "\"
                x: 21.5; y: 10; width: 5; height: 18; radius: 2.5
                color: parent.fill; antialiasing: true
                rotation: -26.6; transformOrigin: Item.Center
            }
        }

        // chấm tâm — đánh dấu chính xác toạ độ GPS thực của drone
        Rectangle {
            anchors.centerIn: parent
            width: 6; height: 6; radius: 3
            color: "#ffffff"
            border.color: telemetry.armed ? Theme.danger : Theme.accent
            border.width: 1.5
        }
    }

    // ── HOME (overlay) ──────────────────────────────────────────────────────
    Item {
        id: homeOverlay
        visible: root.homeCoord !== null
        width: 16; height: 16; z: 41
        property point scr: {
            void map.center; void map.zoomLevel; void map.width; void map.height;
            return root.homeCoord !== null
                ? map.fromCoordinate(root.homeCoord, false) : Qt.point(-10000, -10000);
        }
        x: scr.x - width / 2
        y: scr.y - height / 2
        Rectangle {
            anchors.centerIn: parent; width: 16; height: 16; rotation: 45
            color: "#2937e0a0"; border.color: Theme.accent; border.width: 1.5
        }
        Text {
            text: "HOME"; color: Theme.accent
            font.family: Theme.mono; font.pixelSize: 8; font.letterSpacing: 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.bottom; anchors.topMargin: 2
        }
    }

    // ── GOTO (overlay) ──────────────────────────────────────────────────────
    Item {
        id: gotoOverlay
        visible: root.hasGoto
        width: 16; height: 16; z: 41
        property point scr: {
            void map.center; void map.zoomLevel; void map.width; void map.height;
            return root.hasGoto
                ? map.fromCoordinate(root.gotoCoord, false) : Qt.point(-10000, -10000);
        }
        x: scr.x - width / 2
        y: scr.y - height / 2
        Rectangle {
            anchors.centerIn: parent; width: 16; height: 16; rotation: 45
            color: "transparent"; border.color: Theme.danger; border.width: 1.5
        }
    }

    // ── WP chia sẻ qua cầu nối (overlay, nhấp nháy) ─────────────────────────
    Item {
        id: sharedWpOverlay
        visible: telemetry.sharedWpValid
        width: 42; height: 42; z: 41
        property point scr: {
            void map.center; void map.zoomLevel; void map.width; void map.height;
            return telemetry.sharedWpValid
                ? map.fromCoordinate(QtPositioning.coordinate(telemetry.sharedWpLat, telemetry.sharedWpLon), false)
                : Qt.point(-10000, -10000);
        }
        x: scr.x - width / 2
        y: scr.y - height / 2
        Rectangle {
            id: pulse
            anchors.centerIn: parent
            width: 16; height: 16; radius: 8
            color: "transparent"; border.color: Theme.warn; border.width: 2
            ParallelAnimation {
                running: sharedWpOverlay.visible; loops: Animation.Infinite
                NumberAnimation { target: pulse; property: "scale"; from: 0.5; to: 2.6
                                  duration: 1000; easing.type: Easing.OutQuad }
                NumberAnimation { target: pulse; property: "opacity"; from: 0.9; to: 0.0
                                  duration: 1000 }
            }
        }
        Rectangle {
            id: dot
            anchors.centerIn: parent
            width: 14; height: 14; radius: 7
            color: Theme.warn; border.color: "#ffffff"; border.width: 1
            SequentialAnimation on opacity {
                running: sharedWpOverlay.visible; loops: Animation.Infinite
                NumberAnimation { from: 1.0; to: 0.3; duration: 500 }
                NumberAnimation { from: 0.3; to: 1.0; duration: 500 }
            }
        }
        Text {
            text: "WP"; color: Theme.warn
            font.family: Theme.mono; font.pixelSize: 8; font.bold: true; font.letterSpacing: 1
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottom: parent.top
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
        Text { id: tapTxt; anchors.centerIn: parent; text: "◈ CHẠM BẢN ĐỒ → ĐẶT ĐIỂM ĐẾN"
               color: Theme.danger; font.family: Theme.mono; font.pixelSize: 8; font.letterSpacing: 0.6 }
    }

    // ── toạ độ hiện tại (dưới-trái) ─────────────────────────────────────────
    Text {
        x: 8; anchors.bottom: parent.bottom; anchors.bottomMargin: 8
        text: telemetry.posValid
              ? "VĨ " + telemetry.lat.toFixed(6) + "°  KINH " + telemetry.lon.toFixed(6)
                + "°  CAO " + telemetry.altRel.toFixed(1) + "m  · PHÓNG " + Math.round(map.zoomLevel)
              : "— CHƯA CÓ GPS — PHÓNG " + Math.round(map.zoomLevel)
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
                   text: "ĐÃ ĐẶT ĐIỂM · BAY TỚI?"; color: Theme.text
                   font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 0.4 }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: flyTxt.implicitWidth + 20; height: 22; radius: 5
                color: telemetry.connected ? "#d9ff3b3b" : "transparent"
                border.color: Theme.danger
                opacity: telemetry.connected ? 1 : 0.5
                Text { id: flyTxt; anchors.centerIn: parent; text: "BAY TỚI ĐÂY"
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
                        backend.clearSharedWaypoint(); // đã bay tới — thôi chia sẻ điểm chọn
                    }
                }
            }
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                width: cxTxt.implicitWidth + 16; height: 22; radius: 5
                color: "transparent"; border.color: "#3a3340"
                Text { id: cxTxt; anchors.centerIn: parent; text: "HUỶ"
                       color: "#b9b7c2"; font.family: Theme.mono; font.pixelSize: 10; font.letterSpacing: 0.6 }
                MouseArea { anchors.fill: parent; cursorShape: Qt.PointingHandCursor
                            onClicked: { root.gotoCoord = null; backend.clearSharedWaypoint(); } }
            }
        }
    }

    // ── overlay chẩn đoán khi bản đồ KHÔNG tải được ─────────────────────────
    // Trên Linux/Raspberry Pi map thường trắng vì thiếu plugin Qt Location (OSM)
    // hoặc backend TLS. Thay vì để trắng khó hiểu, hiện rõ lý do + cách khắc phục.
    // - map.error khác NoMap  → dịch vụ bản đồ / plugin lỗi.
    // - hết thời gian mà supportedMapTypes rỗng → plugin OSM chưa được cài/nạp.
    property bool loadTimedOut: false
    Timer { interval: 6000; running: true; repeat: false; onTriggered: root.loadTimedOut = true }

    Rectangle {
        anchors.fill: parent
        z: 60
        color: "#ec050a09"
        visible: map.error !== Map.NoError
                 || (root.loadTimedOut && map.supportedMapTypes.length === 0)

        Column {
            anchors.centerIn: parent
            width: parent.width - 48
            spacing: 10

            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                text: "⚠ BẢN ĐỒ KHÔNG TẢI ĐƯỢC"
                color: Theme.warn; font.family: Theme.mono; font.pixelSize: 15; font.bold: true; font.letterSpacing: 1
            }
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: map.error !== Map.NoError
                      ? ("Lỗi dịch vụ bản đồ: " + map.errorString)
                      : "Thiếu plugin Qt Location (OSM) hoặc backend TLS."
                color: Theme.text; font.family: Theme.mono; font.pixelSize: 11
            }
            Text {
                width: parent.width; horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.WordWrap
                text: "Trên Linux/Raspberry Pi cài các gói:\n"
                      + "sudo apt install qml6-module-qtlocation qml6-module-qtpositioning \\\n"
                      + "    libqt6positioning6-plugins openssl ca-certificates"
                color: Theme.dim; font.family: Theme.mono; font.pixelSize: 10
            }
        }
    }
}
