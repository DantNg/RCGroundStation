#include "mavlink/mavlink_bridge.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QUdpSocket>

namespace gcs::mavlink {

namespace {
// Danh tính GCS khi đóng gói khung chia sẻ (khớp MavlinkLink::kGcsSystem).
constexpr int kGcsSystem = 255;
constexpr int kGcsComponent = MAV_COMP_ID_MISSIONPLANNER;
// type_mask "chỉ vị trí" (ba bit vị trí = 0 = dùng); và "huỷ" (mọi bit = 1).
constexpr uint16_t kPosOnlyMask = 0b0000111111111000;
constexpr uint16_t kClearMask = 0xFFFF;
// Trần hàng đợi khi chưa kết nối (không có ai rút) — chỉ điểm mới nhất là đáng kể.
constexpr size_t kTxQueueCap = 16;

QByteArray toBytes(const MavMessage &msg)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buf, &msg);
    return QByteArray(reinterpret_cast<const char *>(buf), len);
}
}

MavlinkBridge::MavlinkBridge() = default;
MavlinkBridge::~MavlinkBridge() = default;

void MavlinkBridge::configure(bool enabled, uint16_t port, bool allowUplink)
{
    m_port.store(port);
    m_allowUplink.store(allowUplink);
    m_enabled.store(enabled);
    // Bind lại từ đầu ở lần service kế nếu bật lại sau khi từng lỗi; và xả các
    // khung chia sẻ còn tồn khi tắt cầu nối.
    if (!enabled) {
        m_bindFailed = false;
        std::lock_guard<std::mutex> lock(m_txMutex);
        m_txQueue.clear();
    }
}

void MavlinkBridge::queueFrame(const MavMessage &msg)
{
    if (!m_enabled.load())
        return;
    QByteArray bytes = toBytes(msg);
    std::lock_guard<std::mutex> lock(m_txMutex);
    m_txQueue.push_back(std::move(bytes));
    while (m_txQueue.size() > kTxQueueCap)
        m_txQueue.pop_front();
}

void MavlinkBridge::shareWaypoint(double lat, double lon, double altRel)
{
    MavMessage msg;
    mavlink_msg_set_position_target_global_int_pack(kGcsSystem, kGcsComponent, &msg,
        0, 0, 0, MAV_FRAME_GLOBAL_RELATIVE_ALT_INT, kPosOnlyMask,
        static_cast<int32_t>(lat * 1e7), static_cast<int32_t>(lon * 1e7),
        static_cast<float>(altRel), 0, 0, 0, 0, 0, 0, 0, 0);
    queueFrame(msg);
}

void MavlinkBridge::clearWaypoint()
{
    MavMessage msg;
    mavlink_msg_set_position_target_global_int_pack(kGcsSystem, kGcsComponent, &msg,
        0, 0, 0, MAV_FRAME_GLOBAL_RELATIVE_ALT_INT, kClearMask,
        0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    queueFrame(msg);
}

void MavlinkBridge::ensureOpen(const NoticeFn &notice)
{
    if (m_socket || m_bindFailed)
        return;
    auto sock = std::make_unique<QUdpSocket>();
    // Cổng nguồn tạm thời (port 0): máy tính sẽ trả lời về đúng cổng này, còn
    // gói broadcast của ta đi tới cổng đích khác nên không tự dội về đây.
    if (!sock->bind(QHostAddress::AnyIPv4, 0)) {
        m_bindFailed = true;
        if (notice)
            notice(QStringLiteral("Cầu nối: không mở được UDP: %1").arg(sock->errorString()), true);
        return;
    }
    m_socket = std::move(sock);
    if (notice)
        notice(QStringLiteral("Chế độ cầu nối BẬT — broadcast telemetry ra UDP :%1")
                   .arg(m_port.load()),
               false);
}

void MavlinkBridge::service(const InjectSink &inject, const NoticeFn &notice)
{
    const bool want = m_enabled.load();
    if (!want) {
        if (m_socket)
            shutdown();
        return;
    }
    ensureOpen(notice);
    if (!m_socket)
        return;

    // Khung chia sẻ waypoint do luồng giao diện xếp → broadcast ra mạng.
    std::deque<QByteArray> tx;
    {
        std::lock_guard<std::mutex> lock(m_txMutex);
        tx.swap(m_txQueue);
    }
    for (const QByteArray &frame : tx)
        m_socket->writeDatagram(frame, QHostAddress::Broadcast, m_port.load());

    // Gói máy tính gửi về (lệnh/tham số/nhiệm vụ) → tiêm ngược vào phương tiện.
    while (m_socket->hasPendingDatagrams()) {
        const QNetworkDatagram dg = m_socket->receiveDatagram();
        if (m_allowUplink.load() && inject)
            inject(dg.data());
    }
}

void MavlinkBridge::forward(const MavMessage &msg)
{
    if (!m_enabled.load() || !m_socket)
        return;
    m_socket->writeDatagram(toBytes(msg), QHostAddress::Broadcast, m_port.load());
}

void MavlinkBridge::shutdown()
{
    if (m_socket) {
        m_socket->close();
        m_socket.reset();
    }
    m_bindFailed = false;
}

} // namespace gcs::mavlink
