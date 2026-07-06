// Gốc lắp ráp (composition root) — sở hữu mọi hệ con và nối chúng lại.
//
// Bản desktop tương ứng ``GroundStationApp`` của firmware. Giao diện chỉ nói
// chuyện với đối tượng này: đọc telemetry qua snapshot(), rút nhật ký qua
// drainNotices(), gửi lệnh qua commands(), mở/đóng link qua connect()/
// disconnect(). Không có gì ở đây #include Qt-widget.
#pragma once

#include "app/link_manager.h"
#include "app/store.h"
#include "domain/roles.h"
#include "domain/telemetry.h"
#include "interfaces/authority_sink.h"
#include "mavlink/command_service.h"
#include "mavlink/mission_service.h"

#include <deque>
#include <memory>
#include <mutex>
#include <vector>

namespace gcs::app {

class GcsController {
public:
    // Vai trò quyết định quyền của bản dựng này (mặc định: trạm mặt đất/admin).
    explicit GcsController(domain::Role role = domain::Role::GroundStationAdmin);

    domain::Role role() const { return m_role; }
    domain::Authority authority() const { return m_authority; }

    // ── vòng đời kết nối ──────────────────────────────────────────────────────
    void connect(const QString &connectionString, int baud, const QString &label = QString());
    void disconnect();
    bool isConnected() const { return m_linkManager.link() != nullptr; }

    // ── chế độ cầu nối MAVLink (chuyển tiếp qua Wi-Fi) ────────────────────────
    // Uplink (máy tính → phương tiện) chỉ mở khi vai trò của trạm được điều
    // khiển — trạm chỉ-xem vẫn broadcast telemetry nhưng không cho tiêm lệnh về.
    void setBridgeMode(bool enabled, int port);
    bool bridgeEnabled() const { return m_linkManager.bridgeEnabled(); }

    // Chia sẻ / huỷ điểm waypoint operator chọn ra mạng cầu nối (để trạm giám
    // sát thấy). Không tác dụng nếu cầu nối đang tắt.
    void shareWaypoint(double lat, double lon, double altRel) { m_linkManager.shareWaypoint(lat, lon, altRel); }
    void clearSharedWaypoint() { m_linkManager.clearSharedWaypoint(); }

    // ── đọc cho giao diện ──────────────────────────────────────────────────────
    domain::TelemetrySnapshot snapshot() const { return m_store.snapshot(); }
    std::vector<domain::StatusText> drainNotices();

    mavlink::CommandService &commands() { return *m_commands; }
    mavlink::MissionService &mission() { return *m_mission; }

    // Sink lệnh ĐÃ GÁC QUYỀN, đang sống (hoặc nullptr khi chưa kết nối). Mọi
    // tiêu thụ gửi lệnh trực tiếp (vd chính sách bắt bám) đều qua đây, nên quyền
    // được thực thi thống nhất ở một chỗ.
    interfaces::ICommandSink *commandSink()
    {
        return m_linkManager.commandSink() ? &m_guardedSink : nullptr;
    }

private:
    void postNotice(const domain::StatusText &st);

    domain::Role m_role;
    domain::Authority m_authority;
    TelemetryStore m_store;
    std::mutex m_noticesMutex;
    std::deque<domain::StatusText> m_notices;
    LinkManager m_linkManager;
    interfaces::AuthorityGuardedSink m_guardedSink;
    std::unique_ptr<mavlink::CommandService> m_commands;
    std::unique_ptr<mavlink::MissionService> m_mission;
};

} // namespace gcs::app
