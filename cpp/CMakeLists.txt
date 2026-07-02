cmake_minimum_required(VERSION 3.21)
project(LiteGCS LANGUAGES CXX)

# ── Trạm điều khiển mặt đất (Lite Ground Station) — bản C++/Qt ────────────────
# Bản dựng lại bằng C++ của ứng dụng Python/PySide6, tối ưu để chạy trên
# Raspberry Pi. Kiến trúc SOLID được giữ nguyên: domain / interfaces / mavlink /
# app / ui.

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()

find_package(Qt6 REQUIRED COMPONENTS
    Core Gui Widgets Network SerialPort
    Multimedia MultimediaWidgets WebChannel)

# Cesium 3D cần Qt WebEngine. Có trên Linux/Pi (GCC) và MSVC, nhưng KHÔNG có ở
# bản Qt cho MinGW trên Windows. Vì vậy nó là tuỳ chọn: nếu thiếu, quả cầu 3D
# hiện một placeholder còn phần còn lại của ứng dụng vẫn chạy đầy đủ.
find_package(Qt6 QUIET COMPONENTS WebEngineWidgets)

qt_standard_project_setup()

set(SOURCES
    main.cpp

    src/config.cpp

    src/domain/flight_modes.cpp
    src/domain/mission.cpp

    src/mavlink/mavlink_link.cpp
    src/mavlink/decoder.cpp
    src/mavlink/command_service.cpp
    src/mavlink/mission_service.cpp

    src/app/store.cpp
    src/app/link_manager.cpp
    src/app/controller.cpp

    src/ui/theme.cpp
    src/ui/acrylic.cpp
    src/ui/widgets.cpp
    src/ui/overlay_stage.cpp
    src/ui/round_hud.cpp
    src/ui/warning_overlay.cpp
    src/ui/messages_panel.cpp
    src/ui/mode_rail.cpp
    src/ui/waypoint_editor.cpp
    src/ui/connection_bar.cpp
    src/ui/top_bar.cpp
    src/ui/camera_view.cpp
    src/ui/cesium_view.cpp
    src/ui/map_widget.cpp
    src/ui/main_window.cpp
)

qt_add_executable(LiteGCS WIN32 ${SOURCES})

target_include_directories(LiteGCS PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src)

# MAVLink C library (header-only, generated). Treat as SYSTEM so its generator
# warnings don't drown out ours.
target_include_directories(LiteGCS SYSTEM PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mavlink
    ${CMAKE_CURRENT_SOURCE_DIR}/third_party/mavlink/ardupilotmega)

target_link_libraries(LiteGCS PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets Qt6::Network Qt6::SerialPort
    Qt6::Multimedia Qt6::MultimediaWidgets Qt6::WebChannel)

if(Qt6WebEngineWidgets_FOUND)
    message(STATUS "Qt WebEngine tìm thấy — bật quả cầu Cesium 3D")
    target_compile_definitions(LiteGCS PRIVATE HAVE_WEBENGINE)
    target_link_libraries(LiteGCS PRIVATE Qt6::WebEngineWidgets)
else()
    message(WARNING "Không có Qt WebEngine — chế độ 3D sẽ hiển thị placeholder")
endif()

# Vendored Cesium globe + page. Re-use the exact same assets the Python app
# serves; copy them next to the binary so the localhost server can find them.
add_custom_command(TARGET LiteGCS POST_BUILD
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        ${CMAKE_CURRENT_SOURCE_DIR}/../gcs/ui/assets
        $<TARGET_FILE_DIR:LiteGCS>/assets
    COMMENT "Sao chép tài nguyên Cesium (bản đồ 3D)")
