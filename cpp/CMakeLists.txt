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

# QtSerialPort KHÔNG có trong bộ Qt cho Android — cổng nối tiếp phần cứng không
# khả dụng trên Android (chỉ UDP/TCP qua Wi-Fi). Bỏ SerialPort khỏi bản Android và
# biên dịch tắt mã serial bằng GCS_NO_SERIAL (xem mavlink_link.cpp / backend.cpp).
set(GCS_QT_COMPONENTS
    Core Gui Qml Quick QuickControls2
    Location Positioning Multimedia Network)
if(NOT ANDROID)
    list(APPEND GCS_QT_COMPONENTS SerialPort)
endif()

find_package(Qt6 REQUIRED COMPONENTS ${GCS_QT_COMPONENTS})

qt_standard_project_setup(REQUIRES 6.5)

# ── nguồn backend + cầu nối (không có Qt Widgets) ─────────────────────────────
set(SOURCES
    main.cpp

    src/config.cpp

    src/domain/flight_modes.cpp
    src/domain/mission.cpp

    src/interfaces/authority_sink.cpp

    src/mavlink/mavlink_link.cpp
    src/mavlink/mavlink_bridge.cpp
    src/mavlink/decoder.cpp
    src/mavlink/command_service.cpp
    src/mavlink/mission_service.cpp

    src/app/store.cpp
    src/app/link_manager.cpp
    src/app/controller.cpp

    src/bridge/telemetry_view_model.cpp
    src/bridge/backend.cpp
    src/bridge/joystick_controller.cpp
)

# Trên Android, mục tiêu phải là thư viện chia sẻ (androiddeployqt gói thành APK);
# WIN32 chỉ áp cho Windows (ẩn cửa sổ console). qt_add_executable xử lý cả hai.
if(ANDROID)
    qt_add_executable(LiteGCS ${SOURCES})
else()
    qt_add_executable(LiteGCS WIN32 ${SOURCES})
endif()

target_include_directories(LiteGCS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

# Tắt mã cổng nối tiếp trên Android (không có QtSerialPort).
if(ANDROID)
    target_compile_definitions(LiteGCS PRIVATE GCS_NO_SERIAL)
endif()

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
        qml/JoystickPad.qml
        qml/JoystickOverlay.qml
    RESOURCES
        qml/fonts/JetBrainsMono-Regular.ttf
        qml/fonts/ChakraPetch-Medium.ttf
        qml/fonts/ChakraPetch-Bold.ttf
)

target_link_libraries(LiteGCS PRIVATE
    Qt6::Core Qt6::Gui Qt6::Qml Qt6::Quick Qt6::QuickControls2
    Qt6::Location Qt6::Positioning Qt6::Multimedia Qt6::Network)
if(NOT ANDROID)
    target_link_libraries(LiteGCS PRIVATE Qt6::SerialPort)
endif()

# ── định nghĩa nhà cung cấp tile bản đồ (Esri vệ tinh + OSM) ──────────────────
# main.cpp trỏ osm.mapping.providersrepository.address tới thư mục này. Trên
# desktop: copy cạnh binary (POST_BUILD). Trên Android: gói vào assets của APK
# (xem android/ + QT_ANDROID_PACKAGE_SOURCE_DIR) và main.cpp đọc "assets:/providers/".
if(NOT ANDROID)
    add_custom_command(TARGET LiteGCS POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_directory
            ${CMAKE_CURRENT_SOURCE_DIR}/providers
            $<TARGET_FILE_DIR:LiteGCS>/providers
        COMMENT "Sao chép định nghĩa nhà cung cấp tile (Esri vệ tinh / OSM)")
endif()

# ── đóng gói Android (APK qua androiddeployqt) ────────────────────────────────
# Thư mục android/ chứa AndroidManifest.xml (quyền INTERNET cho tile bản đồ + UDP/
# TCP), và assets/providers là bản sao định nghĩa tile để chạy offline-repo. Kit
# Qt cho Android + Android SDK/NDK phải được cài (xem cpp/README.md, mục Android).
if(ANDROID)
    # Đồng bộ providers vào assets của gói Android trước khi cấu hình APK.
    file(COPY ${CMAKE_CURRENT_SOURCE_DIR}/providers
         DESTINATION ${CMAKE_CURRENT_SOURCE_DIR}/android/assets)

    set_target_properties(LiteGCS PROPERTIES
        QT_ANDROID_PACKAGE_SOURCE_DIR ${CMAKE_CURRENT_SOURCE_DIR}/android
        QT_ANDROID_TARGET_SDK_VERSION 34
        QT_ANDROID_MIN_SDK_VERSION 28
        QT_ANDROID_VERSION_NAME "1.0"
        QT_ANDROID_VERSION_CODE 1)
endif()
