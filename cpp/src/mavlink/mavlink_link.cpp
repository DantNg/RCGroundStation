#include "mavlink/mavlink_link.h"

#include <QNetworkDatagram>
#include <QSerialPort>
#include <QTcpSocket>
#include <QUdpSocket>

#include <stdexcept>

namespace gcs::mavlink {

namespace {
// Mặt nạ type_mask: mọi bit bật trừ ba bit vị trí → "chỉ dùng vị trí".
constexpr uint16_t kPosOnlyMask = 0b0000111111111000;

QByteArray toBytes(const MavMessage &msg)
{
    uint8_t buf[MAVLINK_MAX_PACKET_LEN];
    const int len = mavlink_msg_to_send_buffer(buf, &msg);
    return QByteArray(reinterpret_cast<const char *>(buf), len);
}
}

MavlinkLink::MavlinkLink(QString connStr, int baud, QString label)
    : m_connStr(std::move(connStr)), m_baud(baud),
      m_label(label.isEmpty() ? m_connStr : std::move(label))
{
    if (m_connStr.startsWith(QLatin1String("udp")))
        m_kind = Kind::Udp;
    else if (m_connStr.startsWith(QLatin1String("tcp")))
        m_kind = Kind::Tcp;
    else
        m_kind = Kind::Serial;
}

MavlinkLink::~MavlinkLink()
{
    close();
}

void MavlinkLink::open()
{
    // Được gọi trên luồng worker; các socket được tạo tại đây nên chỉ thuộc
    // luồng đó.
    if (m_kind == Kind::Udp) {
        // "udpin:0.0.0.0:14550" → bind và nghe.
        const auto parts = m_connStr.split(':');
        const quint16 port = parts.size() >= 3 ? parts.at(2).toUShort() : 14550;
        m_udp = std::make_unique<QUdpSocket>();
        if (!m_udp->bind(QHostAddress::AnyIPv4, port)) {
            const QString err = m_udp->errorString();
            m_udp.reset();
            throw std::runtime_error(("Không thể bind UDP: " + err).toStdString());
        }
        m_device = m_udp.get();
    } else if (m_kind == Kind::Tcp) {
        // "tcp:127.0.0.1:5760"
        const auto parts = m_connStr.split(':');
        const QString host = parts.size() >= 2 ? parts.at(1) : QStringLiteral("127.0.0.1");
        const quint16 port = parts.size() >= 3 ? parts.at(2).toUShort() : 5760;
        m_tcp = std::make_unique<QTcpSocket>();
        m_tcp->connectToHost(host, port);
        if (!m_tcp->waitForConnected(5000)) {
            const QString err = m_tcp->errorString();
            m_tcp.reset();
            throw std::runtime_error(("Không thể kết nối TCP: " + err).toStdString());
        }
        m_device = m_tcp.get();
    } else {
        m_serial = std::make_unique<QSerialPort>();
        m_serial->setPortName(m_connStr);
        m_serial->setBaudRate(m_baud);
        if (!m_serial->open(QIODevice::ReadWrite)) {
            const QString err = m_serial->errorString();
            m_serial.reset();
            throw std::runtime_error(("Không thể mở serial: " + err).toStdString());
        }
        m_device = m_serial.get();
    }
}

void MavlinkLink::close()
{
    m_device = nullptr;
    if (m_serial) { m_serial->close(); m_serial.reset(); }
    if (m_tcp)    { m_tcp->close(); m_tcp.reset(); }
    if (m_udp)    { m_udp->close(); m_udp.reset(); }
    m_rxBuf.clear();
    m_rxPos = 0;
}

std::optional<MavMessage> MavlinkLink::parseFromBuffer()
{
    while (m_rxPos < m_rxBuf.size()) {
        const uint8_t byte = static_cast<uint8_t>(m_rxBuf.at(m_rxPos++));
        MavMessage msg;
        mavlink_status_t status;
        if (mavlink_parse_char(MAVLINK_COMM_0, byte, &msg, &status)) {
            // nén phần đã tiêu thụ ra khỏi buffer
            m_rxBuf.remove(0, m_rxPos);
            m_rxPos = 0;
            learnTarget(msg);
            return msg;
        }
    }
    m_rxBuf.clear();
    m_rxPos = 0;
    return std::nullopt;
}

std::optional<MavMessage> MavlinkLink::recv(double timeoutS)
{
    // Mọi lệnh gửi đã xếp hàng được đẩy đi tại đây, trên luồng worker.
    drainOutbox();

    if (auto m = parseFromBuffer())
        return m;

    if (!m_device)
        return std::nullopt;
    const int timeoutMs = static_cast<int>(timeoutS * 1000.0);

    if (m_kind == Kind::Udp) {
        if (!m_udp->hasPendingDatagrams()
            && !m_udp->waitForReadyRead(timeoutMs))
            return std::nullopt;
        while (m_udp->hasPendingDatagrams()) {
            const QNetworkDatagram dg = m_udp->receiveDatagram();
            if (m_peerAddr.isNull()) {
                m_peerAddr = dg.senderAddress();
                m_peerPort = dg.senderPort();
            }
            m_rxBuf.append(dg.data());
        }
    } else {
        if (m_device->bytesAvailable() == 0
            && !m_device->waitForReadyRead(timeoutMs))
            return std::nullopt;
        m_rxBuf.append(m_device->readAll());
    }
    return parseFromBuffer();
}

void MavlinkLink::learnTarget(const MavMessage &msg)
{
    // Học danh tính phương tiện từ heartbeat đầu tiên nó gửi. Bỏ qua heartbeat
    // của chính ta và của các thành phần GCS khác.
    if (msg.msgid != MAVLINK_MSG_ID_HEARTBEAT)
        return;
    if (msg.sysid == kGcsSystem)
        return;
    mavlink_heartbeat_t hb;
    mavlink_msg_heartbeat_decode(&msg, &hb);
    if (hb.type == MAV_TYPE_GCS)
        return;
    m_targetSystem.store(msg.sysid);
    m_targetComponent.store(msg.compid);
}

// ── xếp hàng gửi ────────────────────────────────────────────────────────────
void MavlinkLink::enqueue(const MavMessage &msg)
{
    std::lock_guard<std::mutex> lock(m_outboxMutex);
    m_outbox.push_back(toBytes(msg));
}

void MavlinkLink::drainOutbox()
{
    std::deque<QByteArray> pending;
    {
        std::lock_guard<std::mutex> lock(m_outboxMutex);
        pending.swap(m_outbox);
    }
    if (!m_device)
        return;
    for (const QByteArray &bytes : pending) {
        if (m_kind == Kind::Udp) {
            if (!m_peerAddr.isNull())
                m_udp->writeDatagram(bytes, m_peerAddr, m_peerPort);
        } else {
            m_device->write(bytes);
        }
    }
}

std::pair<int, int> MavlinkLink::requireTarget() const
{
    if (!m_device)
        throw std::runtime_error("Link chưa mở");
    const int sys = m_targetSystem.load();
    if (sys == 0)
        throw std::runtime_error("Chưa có heartbeat của phương tiện — không thể gửi lệnh");
    return {sys, m_targetComponent.load()};
}

// ── ICommandSink ────────────────────────────────────────────────────────────
void MavlinkLink::commandLong(int command, float p1, float p2, float p3, float p4,
                              float p5, float p6, float p7, int confirmation)
{
    const auto [sys, comp] = requireTarget();
    MavMessage msg;
    mavlink_msg_command_long_pack(kGcsSystem, kGcsComponent, &msg,
        sys, comp, static_cast<uint16_t>(command),
        static_cast<uint8_t>(confirmation), p1, p2, p3, p4, p5, p6, p7);
    enqueue(msg);
}

void MavlinkLink::setMode(int baseMode, int customMode)
{
    const auto [sys, comp] = requireTarget();
    Q_UNUSED(comp);
    MavMessage msg;
    // SET_MODE cổ điển (Mission Planner gửi; ArduPilot tuân theo)
    mavlink_msg_set_mode_pack(kGcsSystem, kGcsComponent, &msg,
        sys, static_cast<uint8_t>(baseMode), static_cast<uint32_t>(customMode));
    enqueue(msg);
    // đai an toàn thứ hai: MAV_CMD_DO_SET_MODE (QGC/PX4 ưa dùng)
    MavMessage msg2;
    mavlink_msg_command_long_pack(kGcsSystem, kGcsComponent, &msg2,
        sys, m_targetComponent.load(), MAV_CMD_DO_SET_MODE, 0,
        static_cast<float>(baseMode), static_cast<float>(customMode), 0, 0, 0, 0, 0);
    enqueue(msg2);
}

void MavlinkLink::setPositionTargetGlobal(double lat, double lon, double altRel)
{
    const auto [sys, comp] = requireTarget();
    MavMessage msg;
    mavlink_msg_set_position_target_global_int_pack(kGcsSystem, kGcsComponent, &msg,
        0, sys, comp, MAV_FRAME_GLOBAL_RELATIVE_ALT_INT, kPosOnlyMask,
        static_cast<int32_t>(lat * 1e7), static_cast<int32_t>(lon * 1e7),
        static_cast<float>(altRel),
        0, 0, 0, 0, 0, 0, 0, 0);
    enqueue(msg);
}

void MavlinkLink::sendHeartbeat()
{
    if (!m_device)
        return;
    MavMessage msg;
    mavlink_msg_heartbeat_pack(kGcsSystem, kGcsComponent, &msg,
        MAV_TYPE_GCS, MAV_AUTOPILOT_INVALID, 0, 0, 0);
    enqueue(msg);
}

void MavlinkLink::requestDataStreams(int rateHz)
{
    const int sys = m_targetSystem.load();
    if (!m_device || sys == 0)
        return;
    const int comp = m_targetComponent.load();
    const int attHz = std::max(rateHz, 10);
    // ArduPilot gom tin vào các bộ luồng cổ điển. EXTRA1 là ATTITUDE (chân trời
    // nhân tạo) nên được ưu tiên tốc độ cao nhất.
    const struct { int id; int hz; } streams[] = {
        {MAV_DATA_STREAM_EXTRA1, attHz},
        {MAV_DATA_STREAM_EXTRA2, std::max(rateHz / 2, 5)},
        {MAV_DATA_STREAM_EXTRA3, 2},
        {MAV_DATA_STREAM_POSITION, 5},
        {MAV_DATA_STREAM_EXTENDED_STATUS, 2},
    };
    for (const auto &s : streams) {
        MavMessage msg;
        mavlink_msg_request_data_stream_pack(kGcsSystem, kGcsComponent, &msg,
            sys, comp, static_cast<uint8_t>(s.id),
            static_cast<uint16_t>(s.hz), 1);
        enqueue(msg);
    }
    // Đai an toàn cho stack ưa lệnh hiện đại (MAV_CMD_SET_MESSAGE_INTERVAL).
    const struct { int msgId; int hz; } intervals[] = {
        {MAVLINK_MSG_ID_ATTITUDE, attHz},
        {MAVLINK_MSG_ID_VFR_HUD, std::max(rateHz / 2, 5)},
        {MAVLINK_MSG_ID_GLOBAL_POSITION_INT, 5},
    };
    for (const auto &iv : intervals) {
        MavMessage msg;
        mavlink_msg_command_long_pack(kGcsSystem, kGcsComponent, &msg,
            sys, comp, MAV_CMD_SET_MESSAGE_INTERVAL, 0,
            static_cast<float>(iv.msgId),
            static_cast<float>(1000000 / iv.hz), 0, 0, 0, 0, 0);
        enqueue(msg);
    }
}

void MavlinkLink::missionCount(int count, int missionType)
{
    const auto [sys, comp] = requireTarget();
    MavMessage msg;
    mavlink_msg_mission_count_pack(kGcsSystem, kGcsComponent, &msg,
        sys, comp, static_cast<uint16_t>(count),
        static_cast<uint8_t>(missionType));
    enqueue(msg);
}

void MavlinkLink::missionItemInt(const interfaces::MissionItem &item)
{
    const auto [sys, comp] = requireTarget();
    MavMessage msg;
    mavlink_msg_mission_item_int_pack(kGcsSystem, kGcsComponent, &msg,
        sys, comp, static_cast<uint16_t>(item.seq), static_cast<uint8_t>(item.frame),
        static_cast<uint16_t>(item.command), static_cast<uint8_t>(item.current),
        static_cast<uint8_t>(item.autocontinue),
        item.p1, item.p2, item.p3, item.p4,
        item.latI, item.lonI, item.alt,
        static_cast<uint8_t>(item.missionType));
    enqueue(msg);
}

} // namespace gcs::mavlink
