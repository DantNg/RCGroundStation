// Bảng chế độ bay + registry nhỏ, tương ứng ``FlightMode.h`` của firmware.
//
// ``custom_mode`` của MAVLink phụ thuộc autopilot. Ta giải mã bảng ArduPilot
// Copter phổ biến và trả về nhãn số cho trường hợp khác, để giao diện luôn hiển
// thị được thứ gì đó hợp lý. Registry mở để mở rộng (OCP).
#pragma once

#include <QString>
#include <QMap>
#include <vector>
#include <optional>

namespace gcs::domain::flight_modes {

constexpr int MAV_AUTOPILOT_ARDUPILOTMEGA = 3;

// Bảng tên<->custom_mode hai chiều cho một autopilot/phương tiện.
struct ModeTable {
    QString name;
    int autopilot;
    QMap<int, QString> byId;

    std::optional<QString> nameOf(int customMode) const;
    std::optional<int> idOf(const QString &modeName) const;
};

// Chế độ truy cập nhanh xuất hiện dưới dạng nút một chạm trong giao diện.
struct QuickMode {
    QString label;       // chữ ngắn trên nút
    QString modeName;    // tên chuẩn dùng để tra cứu
    QString description; // chú thích (tooltip)
};

const ModeTable &arducopter();

const ModeTable *tableFor(int autopilot);

// Tên người-đọc-được; trả về ``MODE <n>`` như firmware nếu không tra được.
QString modeName(int autopilot, int customMode);

// Giải tên chế độ về custom_mode cho autopilot đã cho.
std::optional<int> modeId(int autopilot, const QString &modeName);

// Thứ tự = thứ tự nút. Ưu tiên của người dùng: LOITER, STAB, ALTH, LAND.
const std::vector<QuickMode> &quickModes();
const std::vector<QuickMode> &extraModes();

} // namespace gcs::domain::flight_modes
