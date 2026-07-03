// Vai trò trong hệ thống phân cấp và quyền hạn kèm theo (Yêu cầu 1).
//
// Hệ thống có nhiều cấp thiết bị dùng chung mã nguồn: trạm mặt đất (admin, có
// quyền ghi đè và điều khiển), tay điều khiển (điều khiển, không ghi đè), và màn
// hình đeo tay (chỉ xem). Quyền là một khái niệm DOMAIN thuần — tách khỏi cả
// giao vận (link) lẫn giao diện — nên có thể kiểm thử độc lập và tái dùng ở mọi
// cấp. Việc thực thi quyền do ``AuthorityGuardedSink`` đảm nhiệm.
#pragma once

#include <QString>

namespace gcs::domain {

enum class Role {
    GroundStationAdmin, // trạm mặt đất: điều khiển + ghi đè
    Controller,         // tay điều khiển: điều khiển, không ghi đè
    WristViewer,        // màn đeo: chỉ xem
};

// Quyền suy ra từ vai trò. ``canControl`` = được gửi lệnh tới phương tiện;
// ``canOverride`` = được ghi đè cấp khác (chỉ admin).
struct Authority {
    bool canControl = false;
    bool canOverride = false;

    static Authority of(Role r)
    {
        switch (r) {
        case Role::GroundStationAdmin: return {true, true};
        case Role::Controller:         return {true, false};
        case Role::WristViewer:        return {false, false};
        }
        return {false, false};
    }
};

inline QString roleLabel(Role r)
{
    switch (r) {
    case Role::GroundStationAdmin: return QStringLiteral("Trạm mặt đất (Admin)");
    case Role::Controller:         return QStringLiteral("Tay điều khiển");
    case Role::WristViewer:        return QStringLiteral("Màn đeo (Chỉ xem)");
    }
    return QStringLiteral("Không rõ");
}

// Chuỗi bền vững để lưu cấu hình (ổn định, không việt hoá).
inline QString roleToString(Role r)
{
    switch (r) {
    case Role::GroundStationAdmin: return QStringLiteral("admin");
    case Role::Controller:         return QStringLiteral("controller");
    case Role::WristViewer:        return QStringLiteral("viewer");
    }
    return QStringLiteral("admin");
}

inline Role roleFromString(const QString &s)
{
    if (s == QLatin1String("controller")) return Role::Controller;
    if (s == QLatin1String("viewer"))     return Role::WristViewer;
    return Role::GroundStationAdmin;
}

} // namespace gcs::domain
