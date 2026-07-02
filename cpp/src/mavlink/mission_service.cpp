#include "mavlink/mission_service.h"

namespace gcs::mavlink {

using domain::Severity;
using domain::StatusText;
using domain::Waypoint;
using domain::nowMs;
using interfaces::MissionItem;

MissionService::MissionService(LinkProvider link, NoticeSink notify)
    : m_link(std::move(link)), m_notify(std::move(notify))
{
}

void MissionService::upload(const std::vector<Waypoint> &waypoints,
                            std::optional<double> takeoffAlt)
{
    auto *sink = m_link();
    if (!sink) {
        error(QStringLiteral("Chưa kết nối — bỏ qua tải nhiệm vụ"));
        return;
    }
    if (waypoints.empty()) {
        error(QStringLiteral("Thêm ít nhất một waypoint trước khi tải lên"));
        return;
    }
    auto items = buildItems(waypoints, takeoffAlt);
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        m_items = items;
        m_active = true;
    }
    try {
        sink->missionCount(static_cast<int>(items.size()));
        info(QStringLiteral("Đang tải nhiệm vụ: %1 mục (%2 waypoint + cất cánh)…")
                 .arg(items.size()).arg(waypoints.size()));
    } catch (const std::exception &e) {
        {
            std::lock_guard<std::mutex> lock(m_mutex);
            m_active = false;
        }
        error(QStringLiteral("Tải nhiệm vụ không khởi động được: ") + QString::fromUtf8(e.what()));
    }
}

void MissionService::onMessage(const MavMessage &msg)
{
    if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST_INT) {
        mavlink_mission_request_int_t m;
        mavlink_msg_mission_request_int_decode(&msg, &m);
        sendItem(m.seq);
    } else if (msg.msgid == MAVLINK_MSG_ID_MISSION_REQUEST) {
        mavlink_mission_request_t m;
        mavlink_msg_mission_request_decode(&msg, &m);
        sendItem(m.seq);
    } else if (msg.msgid == MAVLINK_MSG_ID_MISSION_ACK) {
        onAck(msg);
    }
}

void MissionService::sendItem(int seq)
{
    MissionItem item;
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_active || seq < 0 || seq >= static_cast<int>(m_items.size()))
            return;
        item = m_items[seq];
    }
    auto *sink = m_link();
    if (!sink)
        return;
    try {
        sink->missionItemInt(item);
    } catch (const std::exception &e) {
        error(QStringLiteral("Gửi mục nhiệm vụ %1 thất bại: %2").arg(seq).arg(QString::fromUtf8(e.what())));
    }
}

void MissionService::onAck(const MavMessage &msg)
{
    {
        std::lock_guard<std::mutex> lock(m_mutex);
        if (!m_active)
            return;
        m_active = false;
    }
    mavlink_mission_ack_t m;
    mavlink_msg_mission_ack_decode(&msg, &m);
    if (m.type == MAV_MISSION_ACCEPTED)
        m_notify(StatusText(Severity::Notice, QStringLiteral("Tải nhiệm vụ hoàn tất ✓"), nowMs(), true));
    else
        error(QStringLiteral("Phương tiện từ chối nhiệm vụ (kết quả MAV_MISSION %1)").arg(m.type));
}

std::vector<MissionItem> MissionService::buildItems(
    const std::vector<Waypoint> &waypoints, std::optional<double> takeoffAlt) const
{
    const Waypoint &first = waypoints.front();
    const double toAlt = takeoffAlt.value_or(first.alt);
    std::vector<MissionItem> items;

    auto add = [&](int command, int current, double lat, double lon, double alt) {
        MissionItem it;
        it.seq = static_cast<int>(items.size());
        it.frame = MAV_FRAME_GLOBAL_RELATIVE_ALT_INT;
        it.command = command;
        it.current = current;
        it.autocontinue = 1;
        it.p1 = it.p2 = it.p3 = it.p4 = 0.0f;
        it.latI = static_cast<int32_t>(lat * 1e7);
        it.lonI = static_cast<int32_t>(lon * 1e7);
        it.alt = static_cast<float>(alt);
        it.missionType = 0;
        items.push_back(it);
    };

    // seq 0 — chỗ giữ home (autopilot ghi đè bằng home của nó)
    add(MAV_CMD_NAV_WAYPOINT, 1, first.lat, first.lon, 0.0);
    // seq 1 — cất cánh thẳng lên độ cao waypoint đầu
    add(MAV_CMD_NAV_TAKEOFF, 0, 0.0, 0.0, toAlt);
    // tuyến đã lập
    for (const auto &wp : waypoints)
        add(MAV_CMD_NAV_WAYPOINT, 0, wp.lat, wp.lon, wp.alt);
    return items;
}

void MissionService::info(const QString &text)
{
    m_notify(StatusText(Severity::Notice, text, nowMs(), true));
}

void MissionService::error(const QString &text)
{
    m_notify(StatusText(Severity::Error, text, nowMs(), true));
}

} // namespace gcs::mavlink
