// Gốc lắp ráp (composition root) — sở hữu mọi hệ con và nối chúng lại.
//
// Bản desktop tương ứng ``GroundStationApp`` của firmware. Giao diện chỉ nói
// chuyện với đối tượng này: đọc telemetry qua snapshot(), rút nhật ký qua
// drainNotices(), gửi lệnh qua commands(), mở/đóng link qua connect()/
// disconnect(). Không có gì ở đây #include Qt-widget.
#pragma once

#include "app/link_manager.h"
#include "app/store.h"
#include "domain/telemetry.h"
#include "mavlink/command_service.h"
#include "mavlink/mission_service.h"

#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace gcs::app {

class GcsController {
public:
    GcsController();

    // ── vòng đời kết nối ──────────────────────────────────────────────────────
    void connect(const QString &connectionString, int baud, const QString &label = QString());
    void disconnect();
    bool isConnected() const { return m_linkManager.link() != nullptr; }

    // ── đọc cho giao diện ──────────────────────────────────────────────────────
    domain::TelemetrySnapshot snapshot() const { return m_store.snapshot(); }
    std::vector<domain::StatusText> drainNotices();

    mavlink::CommandService &commands() { return *m_commands; }
    mavlink::MissionService &mission() { return *m_mission; }

private:
    void postNotice(const domain::StatusText &st);

    TelemetryStore m_store;
    std::mutex m_noticesMutex;
    std::deque<domain::StatusText> m_notices;
    LinkManager m_linkManager;
    std::unique_ptr<mavlink::CommandService> m_commands;
    std::unique_ptr<mavlink::MissionService> m_mission;
};

} // namespace gcs::app
