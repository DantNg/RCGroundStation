#include "mavlink/mavlink_bridge.h"

#include <QHostAddress>
#include <QNetworkDatagram>
#include <QUdpSocket>

namespace gcs::mavlink {

namespace {
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
    // Bind lại từ đầu ở lần service kế nếu bật lại sau khi từng lỗi.
    if (!enabled)
        m_bindFailed = false;
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
