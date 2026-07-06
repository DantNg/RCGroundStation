// Cài đặt kết nối do người dùng chọn, lưu vào một file JSON nhỏ.
//
// Bản desktop tương ứng với ``AppConfig`` (NVS) của firmware. Cố tình giữ nhỏ:
// nó nhớ đường truyền vừa dùng để lần sau kết nối chỉ một cú nhấp.
#pragma once

#include <QString>

namespace gcs {

struct AppConfig {
    QString connectionType = QStringLiteral("serial"); // "serial" | "udp" | "tcp"
    QString serialPort;
    int baud = 115200;
    int udpPort = 14550;
    QString tcpHost = QStringLiteral("127.0.0.1");
    int tcpPort = 5760;

    // Vai trò của bản dựng này trong hệ phân cấp: "admin" | "controller" |
    // "viewer". Quyết định quyền điều khiển (xem domain/roles.h).
    QString role = QStringLiteral("admin");

    // ── chế độ cầu nối (chuyển tiếp MAVLink ra Wi-Fi cho máy tính đọc) ─────────
    bool bridgeEnabled = false;
    int bridgePort = 14550; // cổng UDP broadcast (mặc định GCS của Mission Planner)

    // Chuỗi/nhãn kết nối cho transport đã chọn.
    QString connectionString() const;
    QString label() const;

    // ── lưu trữ ──────────────────────────────────────────────────────────────
    static AppConfig load();
    void save() const;
};

} // namespace gcs
