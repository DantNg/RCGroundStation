// Các đối tượng giá trị thuần mô tả trạng thái phương tiện bay để hiển thị.
//
// Đây là hợp đồng giữa bộ giải mã MAVLink (ghi) và giao diện (đọc). Tương ứng
// với ``domain/telemetry.py`` của bản Python. Các struct chỉ mang dữ liệu, được
// sao chép theo giá trị.
#pragma once

#include <QString>
#include <cstdint>
#include <chrono>

namespace gcs::domain {

// Mili-giây đơn điệu (monotonic) — tương đương ``millis()`` của Arduino.
inline int64_t nowMs()
{
    using namespace std::chrono;
    return duration_cast<milliseconds>(steady_clock::now().time_since_epoch()).count();
}

// Mức độ nghiêm trọng của STATUSTEXT (MAV_SEVERITY); giá trị nhỏ = nặng nhất.
enum class Severity : int {
    Emergency = 0,
    Alert = 1,
    Critical = 2,
    Error = 3,
    Warning = 4,
    Notice = 5,
    Info = 6,
    Debug = 7,
};

inline QString severityLabel(Severity s)
{
    switch (s) {
    case Severity::Emergency: return QStringLiteral("Emergency");
    case Severity::Alert:     return QStringLiteral("Alert");
    case Severity::Critical:  return QStringLiteral("Critical");
    case Severity::Error:     return QStringLiteral("Error");
    case Severity::Warning:   return QStringLiteral("Warning");
    case Severity::Notice:    return QStringLiteral("Notice");
    case Severity::Info:      return QStringLiteral("Info");
    case Severity::Debug:     return QStringLiteral("Debug");
    }
    return QStringLiteral("Info");
}

// Từ ATTITUDE (radian).
struct Attitude {
    double roll = 0.0;
    double pitch = 0.0;
    double yaw = 0.0;
    int64_t updatedMs = 0;
};

// Từ SYS_STATUS / BATTERY_STATUS.
struct Battery {
    double voltage = 0.0;   // V
    double current = 0.0;   // A
    int remaining = -1;     // %, -1 = chưa biết
    int64_t updatedMs = 0;
};

// Từ GLOBAL_POSITION_INT (+ GPS_RAW_INT cho chất lượng fix).
struct GeoPosition {
    double lat = 0.0;        // độ
    double lon = 0.0;        // độ
    double altMsl = 0.0;     // m
    double altRel = 0.0;     // m
    double headingDeg = 0.0; // độ
    bool valid = false;
    int64_t updatedMs = 0;
};

// Dữ liệu không khí từ VFR_HUD.
struct Vfr {
    double airspeed = 0.0;    // m/s
    double groundspeed = 0.0; // m/s
    double climb = 0.0;       // m/s
    int throttle = 0;         // %
    int64_t updatedMs = 0;
};

// Chất lượng GPS từ GPS_RAW_INT.
struct GpsInfo {
    int fixType = 0;         // 0-1 không, 2 = 2D, 3 = 3D, ...
    int satellites = 0;
    double hdop = 0.0;
    int64_t updatedMs = 0;

    QString fixLabel() const
    {
        switch (fixType) {
        case 0: return QStringLiteral("Không GPS");
        case 1: return QStringLiteral("Chưa fix");
        case 2: return QStringLiteral("2D");
        case 3: return QStringLiteral("3D");
        case 4: return QStringLiteral("DGPS");
        case 5: return QStringLiteral("RTK Float");
        case 6: return QStringLiteral("RTK Fixed");
        default: return QStringLiteral("Fix %1").arg(fixType);
        }
    }
};

// Chế độ bay / trạng thái arm từ HEARTBEAT.
struct FlightModeInfo {
    int baseMode = 0;
    uint32_t customMode = 0;
    int mavType = 0;         // MAV_TYPE_*
    int autopilot = 0;       // MAV_AUTOPILOT_*
    int systemStatus = 0;    // MAV_STATE_*
    bool armed = false;
    int64_t updatedMs = 0;
};

// Điểm waypoint được một trạm KHÁC chia sẻ qua chế độ cầu nối (Wi-Fi). Trạm cầm
// tay chọn điểm trên bản đồ → broadcast SET_POSITION_TARGET_GLOBAL_INT; trạm
// giám sát (máy tính) nhận, dựng marker nhấp nháy tại đây. ``valid=false`` nghĩa
// là điểm đã bị huỷ chọn.
struct SharedWaypoint {
    double lat = 0.0;
    double lon = 0.0;
    double altRel = 0.0;
    bool valid = false;
    int64_t updatedMs = 0;
};

// Sức khoẻ đường truyền phía nhận (do link worker duy trì).
struct LinkStats {
    uint64_t framesReceived = 0;
    uint64_t bytesReceived = 0;
    uint64_t parseErrors = 0;
    int64_t lastFrameMs = 0;
    bool linkUp = false;
    QString sourceName = QStringLiteral("—");
};

// Một dòng STATUSTEXT (dùng cho nhật ký kiểu Mission Planner).
struct StatusText {
    Severity severity = Severity::Info;
    QString text;
    int64_t updatedMs = 0;
    bool valid = false;

    StatusText() = default;
    StatusText(Severity sev, QString t, int64_t ms, bool v)
        : severity(sev), text(std::move(t)), updatedMs(ms), valid(v) {}
};

// Ảnh chụp đầy đủ được sao chép nguyên tử ra khỏi TelemetryStore.
struct TelemetrySnapshot {
    Attitude attitude;
    Battery battery;
    GeoPosition position;
    GpsInfo gps;
    Vfr vfr;
    FlightModeInfo mode;
    LinkStats link;
    StatusText status;
    SharedWaypoint sharedWaypoint;
    bool heartbeatSeen = false;
    int64_t lastHeartbeatMs = 0;
};

} // namespace gcs::domain
