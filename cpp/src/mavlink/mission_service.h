// Tải nhiệm vụ — điều khiển giao thức MISSION của MAVLink tới phương tiện thật.
//
// Bản đồ 2D/3D lập một danh sách waypoint; lớp này biến chúng thành một nhiệm vụ
// ArduCopter bay được (một điểm home tạm, một lệnh cất cánh, rồi các waypoint)
// và tải lên bằng bắt tay chuẩn:
//   GCS → MISSION_COUNT(n)
//   xe  → MISSION_REQUEST_INT(seq)  (lặp)
//   GCS → MISSION_ITEM_INT(seq, …)
//   xe  → MISSION_ACK(result)
//
// ``upload()`` gọi trên luồng giao diện; ``onMessage()`` được luồng worker nạp
// mỗi tin đến, nên phần trạng thái chia sẻ nhỏ được bảo vệ bằng một khoá.
#pragma once

#include "domain/mission.h"
#include "domain/telemetry.h"
#include "interfaces/ports.h"

#include <functional>
#include <mutex>
#include <optional>
#include <vector>

namespace gcs::mavlink {

using LinkProvider = std::function<interfaces::ICommandSink *()>;
using NoticeSink = std::function<void(const domain::StatusText &)>;

class MissionService {
public:
    MissionService(LinkProvider link, NoticeSink notify);

    // Dựng + bắt đầu tải nhiệm vụ từ các waypoint đã lập.
    void upload(const std::vector<domain::Waypoint> &waypoints,
                std::optional<double> takeoffAlt = std::nullopt);

    // Các tin giao thức đến (luồng worker).
    void onMessage(const MavMessage &msg);

private:
    void sendItem(int seq);
    void onAck(const MavMessage &msg);
    std::vector<interfaces::MissionItem> buildItems(
        const std::vector<domain::Waypoint> &waypoints,
        std::optional<double> takeoffAlt) const;
    void info(const QString &text);
    void error(const QString &text);

    LinkProvider m_link;
    NoticeSink m_notify;
    std::mutex m_mutex;
    std::vector<interfaces::MissionItem> m_items;
    bool m_active = false;
};

} // namespace gcs::mavlink
