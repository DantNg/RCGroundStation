// Các cổng (interface) tách theo trách nhiệm (Interface Segregation).
//
// ``ITelemetryLink`` là phía *đọc* — mở kết nối và lấy các tin MAVLink đã giải
// mã. ``ICommandSink`` là phía *ghi* — gửi lệnh tới phương tiện. Một adapter cụ
// thể có thể hiện thực cả hai, nhưng bên đọc không phụ thuộc vào phương thức
// lệnh và ngược lại.
#pragma once

#include "mavlink/mav_message.h"

#include <QByteArray>
#include <QString>
#include <optional>
#include <utility>
#include <vector>
#include <cstdint>

namespace gcs::interfaces {

// Một mục nhiệm vụ đã dựng sẵn để tải lên (giao thức MISSION).
struct MissionItem {
    int seq;
    int frame;
    int command;
    int current;
    int autocontinue;
    float p1, p2, p3, p4;
    int32_t latI, lonI;
    float alt;
    int missionType = 0;
};

// Phía đọc: một kết nối trả về các tin MAVLink đã giải mã.
class ITelemetryLink {
public:
    virtual ~ITelemetryLink() = default;

    virtual bool isOpen() const = 0;

    // Mở transport nền. Ném ngoại lệ (std::runtime_error) khi thất bại.
    virtual void open() = 0;

    // Đóng transport. Gọi nhiều lần vẫn an toàn.
    virtual void close() = 0;

    // Chặn tối đa ``timeoutS`` giây chờ tin kế tiếp; std::nullopt nếu hết giờ.
    virtual std::optional<MavMessage> recv(double timeoutS) = 0;

    // Nhãn ngắn cho con người (vd "COM5" hoặc "udp:14550").
    virtual QString sourceName() const = 0;

    // (system, component) của phương tiện, hoặc (0, 0) khi chưa biết.
    virtual std::pair<int, int> target() const = 0;

    // Yêu cầu phương tiện phát các luồng telemetry (ATTITUDE, vị trí, trạng thái).
    virtual void requestDataStreams(int rateHz = 12) = 0;
};

// Phía ghi: các lệnh MAVLink cấp thấp gửi tới mục tiêu đang hoạt động.
class ICommandSink {
public:
    virtual ~ICommandSink() = default;

    // Gửi COMMAND_LONG với tối đa 7 tham số float.
    virtual void commandLong(int command,
                             float p1 = 0, float p2 = 0, float p3 = 0, float p4 = 0,
                             float p5 = 0, float p6 = 0, float p7 = 0,
                             int confirmation = 0) = 0;

    // Gửi SET_MODE.
    virtual void setMode(int baseMode, int customMode) = 0;

    // Ra lệnh mục tiêu vị trí GUIDED ("Bay đến đây"). lat/lon độ, altRel m.
    virtual void setPositionTargetGlobal(double lat, double lon, double altRel) = 0;

    // Thông báo trạm mặt đất này cho phương tiện (giữ bộ đếm GCS-failsafe).
    virtual void sendHeartbeat() = 0;

    // ── nguyên thủy tải nhiệm vụ (giao thức MISSION) ─────────────────────────
    virtual void missionCount(int count, int missionType = 0) = 0;
    virtual void missionItemInt(const MissionItem &item) = 0;

    // Chuyển tiếp một khung MAVLink thô (đã đóng gói đầy đủ) tới phương tiện —
    // dùng cho chế độ cầu nối khi tiêm gói từ một GCS ngoài (máy tính). Không
    // diễn giải nội dung, không kiểm tra mục tiêu.
    virtual void sendRawFrame(const QByteArray &frame) = 0;
};

} // namespace gcs::interfaces
