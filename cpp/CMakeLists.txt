cmake_minimum_required(VERSION 3.21)
project(LiteGCS LANGUAGES CXX)

# ── Trạm điều khiển mặt đất (Lite Ground Station) — bản C++/Qt, UI QML ─────────
# Lớp UI dựng bằng QML/Qt Quick theo mockup "GroundController Handheld" (bản đồ 2D
# vệ tinh + PFD + camera FPV). Backend C++ giữ nguyên kiến trúc SOLID: domain /
# interfaces / mavlink / app; hai lớp cầu nối (src/bridge) lộ ra cho QML.
#
# Bản Qt Widgets cũ vẫn còn trên đĩa (src/ui, main_widgets.cpp) để tham chiếu,
# nhưng KHÔNG còn được build — QML là giao diện chính.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Qml Quick QuickControls2
    Location Positioning Multimedia
    Network SerialPort)

qt_standard_project_setup(REQUIRES 6.5)

# ── nguồn backend + cầu nối (không có Qt Widgets) ─────────────────────────────
set(SOURCES
    main.cpp

    src/config.cpp

    src/domain/flight_modes.cpp
    src/domain/mission.cpp

    src/interfaces/authority_sink.cpp

    src/mavlink/mavlink_link.cpp
    src/mavlink/decoder.cpp
    src/mavlink/command_service.cpp
    src/mavlink/mission_service.cpp

    src/app/store.cpp
    src/app/link_manager.cpp
    src/app/controller.cpp

    src/bridge/telemetry_view_model.cpp
    src/bridge/backend.cpp
)

qt_add_executable(LiteGCS WIN32 ${SOURCES})

target_include_directories(LiteGCS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

# MAVLink C library (header-only, generated). Treat as SYSTEM so its generator
# warnings don't drown out ours.
target_include_directories(LiteGCS SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mavlink
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mavlink/ardupilotmega)

# ── module QML "GroundCtrl" ───────────────────────────────────────────────────
# RESOURCE_PREFIX /qt/qml là BẮT BUỘC để loadFromModule("GroundCtrl","Main") tìm
# được type Main; Theme là singleton (pragma Singleton + thuộc tính dưới đây).
set_source_files_properties(qml/Theme.qml PROPERTIES QT_QML_SINGLETON_TYPE TRUE)

qt_add_qml_module(LiteGCS
    URI GroundCtrl
    VERSION 1.0
    RESOURCE_PREFIX /qt/qml
    QML_FILES
        qml/Theme.qml
        qml/Main.qml
        qml/StatusBar.qml
        qml/MainView.qml
        qml/SatelliteMap.qml
        qml/CameraView.qml
        qml/HudView.qml
        qml/ModeRail.qml
        qml/TelemetryStrip.qml
        qml/ConnectionCard.qml
        qml/WarningBanners.qml
    RESOURCES
        qml/fonts/JetBrainsMono-Regular.ttf
        qml/fonts/ChakraPetch-Medium.ttf
        qml/fonts/ChakraPetch-Bold.ttf
)

target_link_libraries(LiteGCS PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Location Qt6::Positioning Qt6::Multimedia
    Qt6::Network Qt6::SerialPort)

# ── định nghĩa nhà cung cấp tile bản đồ (Esri vệ tinh + OSM) ──────────────────
# main.cpp trỏ osm.mapping.providersrepository.address tới thư mục này (cạnh
# binary); plugin OSM đọc file "satellite"/"street" từ đây.
add_custom_command(TARGET LiteGCS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/providers
        $<TARGET_FILE_DIR:LiteGCS>/providers
    COMMENT "Sao chép định nghĩa nhà cung cấp tile (Esri vệ tinh / OSM)")
