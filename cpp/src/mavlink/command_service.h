// Lệnh phương tiện cấp cao (arm/disarm, đổi chế độ, cất cánh, bay-đến).
//
// Đây là tầng *chính sách* trên ``ICommandSink``: nó biết arm là
// ``MAV_CMD_COMPONENT_ARM_DISARM``, "LOITER" ánh xạ tới custom_mode của Copter,
// và "Bay đến đây" là mục tiêu vị trí GUIDED — nhưng không biết byte đến phương
// tiện bằng cách nào. Nó lấy sink qua một provider để không giữ tham chiếu cũ
// qua các lần kết nối lại.
#pragma once

#include "domain/telemetry.h"
#include "interfaces/ports.h"

#include <cstdint>
#include <functional>

namespace gcs::mavlink {

using LinkProvider = std::function<interfaces::ICommandSink *()>;
using StateProvider = std::function<domain::TelemetrySnapshot()>;
using NoticeSink = std::function<void(const domain::StatusText &)>;

class CommandService {
public:
    CommandService(LinkProvider link, StateProvider state, NoticeSink notify);

    void arm(bool force = false);
    void disarm(bool force = false);
    void setModeByName(const QString &modeName);
    void takeoff(double altitudeM);
    void flyTo(double lat, double lon, double altRel);
    void startMission();

private:
    void sendArm(bool arm, bool force);
    void ensureGuided(interfaces::ICommandSink *sink);
    void switchMode(interfaces::ICommandSink *sink, const QString &modeName, bool announce);
    // Gửi lệnh NAV_TAKEOFF thô (giả định đã ở GUIDED và đã ARM).
    void sendTakeoff(double altitudeM);
    // Chờ heartbeat xác nhận đã vào GUIDED rồi mới cất cánh (poll tới deadline).
    void waitForGuidedThenTakeoff(double altitudeM, int64_t deadlineMs);
    interfaces::ICommandSink *requireLink();
    void info(const QString &text);
    void error(const QString &text);

    LinkProvider m_link;
    StateProvider m_state;
    NoticeSink m_notify;
};

} // namespace gcs::mavlink
