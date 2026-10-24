#include "rc/rc_link.h"

#include "domain/telemetry.h"

#include <QSerialPort>
#include <QTimer>

namespace gcs::rc {

namespace {
constexpr int kRcStateVersion = 1;
constexpr int kRcStateLen = 48;
constexpr quint8 kFlagCalib = 0x01;
constexpr quint8 kFlagElrsOut = 0x02;
constexpr int kMaxPayload = 250;
constexpr int64_t kConnectedMs = 500;
constexpr int kReopenMs = 2000;
constexpr int kAckTimeoutMs = 500;
constexpr int kSaveTimeoutMs = 2000;

quint8 checksum(quint8 cmd, const QByteArray &data)
{
    quint8 cs = cmd ^ static_cast<quint8>(data.size());
    for (char b : data)
        cs ^= static_cast<quint8>(b);
    return cs;
}

int u16be(const QByteArray &d, int off)
{
    return (static_cast<quint8>(d[off]) << 8) | static_cast<quint8>(d[off + 1]);
}
} // namespace

QString statusName(int status)
{
    switch (status) {
    case kStatusTimeout: return QStringLiteral("TIMEOUT");
    case 0x00: return QStringLiteral("OK");
    case 0x01: return QStringLiteral("ERR");
    case 0x02: return QStringLiteral("BAD_CRC");
    case 0x03: return QStringLiteral("BAD_LEN");
    case 0x04: return QStringLiteral("FLASH");
    case 0x05: return QStringLiteral("RANGE");
    case 0x06: return QStringLiteral("STATE");
    default: return QStringLiteral("0x%1").arg(status, 2, 16, QLatin1Char('0'));
    }
}

std::optional<RcState> decodeRcState(const QByteArray &data)
{
    if (data.size() != kRcStateLen || static_cast<quint8>(data[0]) != kRcStateVersion)
        return std::nullopt;
    RcState st;
    st.seq = static_cast<quint8>(data[1]);
    for (int i = 0; i < 8; ++i)
        st.controlsUs[i] = u16be(data, 2 + 2 * i);
    for (int i = 0; i < 8; ++i)
        st.channelsUs[i] = u16be(data, 18 + 2 * i);
    for (int i = 0; i < 6; ++i)
        st.rawAdc[i] = u16be(data, 34 + 2 * i);
    const quint8 sw = static_cast<quint8>(data[46]);
    for (int i = 0; i < 4; ++i)
        st.switchPos[i] = (sw >> (2 * i)) & 0x03;
    const quint8 flags = static_cast<quint8>(data[47]);
    st.calibLoaded = flags & kFlagCalib;
    st.elrsOutput = flags & kFlagElrsOut;
    st.timestampMs = domain::nowMs();
    return st;
}

QByteArray buildFrame(quint8 cmd, const QByteArray &data)
{
    QByteArray f;
    f.reserve(data.size() + 4);
    f.append(static_cast<char>(kHeader));
    f.append(static_cast<char>(cmd));
    f.append(static_cast<char>(data.size()));
    f.append(data);
    f.append(static_cast<char>(checksum(cmd, data)));
    return f;
}

std::vector<std::pair<quint8, QByteArray>> FrameParser::feed(const QByteArray &chunk)
{
    std::vector<std::pair<quint8, QByteArray>> out;
    m_buf += chunk;
    while (true) {
        const int start = m_buf.indexOf(static_cast<char>(kHeader));
        if (start < 0) {
            m_buf.clear();
            break;
        }
        if (start > 0)
            m_buf.remove(0, start);
        if (m_buf.size() < 3)
            break;
        const quint8 cmd = static_cast<quint8>(m_buf[1]);
        const int len = static_cast<quint8>(m_buf[2]);
        if (len > kMaxPayload) {
            m_buf.remove(0, 1);
            continue;
        }
        const int total = 3 + len + 1;
        if (m_buf.size() < total)
            break;
        const QByteArray data = m_buf.mid(3, len);
        if (checksum(cmd, data) == static_cast<quint8>(m_buf[total - 1])) {
            m_buf.remove(0, total);
            out.emplace_back(cmd, data);
        } else {
            m_buf.remove(0, 1); // checksum sai: header chỉ là một byte dữ liệu
        }
    }
    return out;
}

RcLink::RcLink(QObject *parent) : QObject(parent)
{
    m_port = new QSerialPort(this);
    connect(m_port, &QSerialPort::readyRead, this, &RcLink::onReadyRead);
    connect(m_port, &QSerialPort::errorOccurred, this, [this](QSerialPort::SerialPortError e) {
        // ResourceError = thiết bị biến mất (rút cáp / reset UART).
        if (e == QSerialPort::ResourceError || e == QSerialPort::PermissionError)
            closePort(m_port->errorString());
    });

    m_reopenTimer = new QTimer(this);
    m_reopenTimer->setInterval(kReopenMs);
    connect(m_reopenTimer, &QTimer::timeout, this, &RcLink::tryOpen);

    m_ackTimer = new QTimer(this);
    m_ackTimer->setSingleShot(true);
    connect(m_ackTimer, &QTimer::timeout, this, [this] { finishCurrent(kStatusTimeout, {}); });
}

RcLink::~RcLink()
{
    stop();
}

void RcLink::start(const QString &portName, int baud)
{
    stop();
    m_portName = portName;
    m_baud = baud;
    if (m_portName.isEmpty())
        return;
    tryOpen();
    m_reopenTimer->start();
}

void RcLink::stop()
{
    m_reopenTimer->stop();
    closeNow();
}

bool RcLink::isOpen() const
{
    return m_port->isOpen();
}

bool RcLink::connected() const
{
    return m_state && domain::nowMs() - m_state->timestampMs < kConnectedMs;
}

void RcLink::tryOpen()
{
    if (m_port->isOpen())
        return;
    m_port->setPortName(m_portName);
    m_port->setBaudRate(m_baud);
    m_port->setDataBits(QSerialPort::Data8);
    m_port->setParity(QSerialPort::NoParity);
    m_port->setStopBits(QSerialPort::OneStop);
    m_port->setFlowControl(QSerialPort::NoFlowControl);
    if (!m_port->open(QIODevice::ReadWrite)) {
        m_lastError = m_port->errorString();
        return;
    }
    m_lastError.clear();
    m_port->clear(QSerialPort::Input);
    m_parser = FrameParser();
    m_lastSeq.reset();
    send(kModeRcState); // đảm bảo STM32 đang stream RC_STATE (không có ACK)
    emit openChanged(true);
}

void RcLink::closePort(const QString &reason)
{
    // Đóng trễ: errorOccurred có thể phát ra ngay trong lúc QSerialPort đang xử lý.
    m_lastError = reason;
    QTimer::singleShot(0, this, &RcLink::closeNow);
}

void RcLink::closeNow()
{
    if (!m_port->isOpen())
        return;
    m_port->close();
    m_queue.clear();
    if (m_inFlight)
        finishCurrent(kStatusTimeout, {});
    emit openChanged(false);
}

void RcLink::onReadyRead()
{
    for (auto &[cmd, data] : m_parser.feed(m_port->readAll()))
        handleFrame(cmd, data);
}

void RcLink::handleFrame(quint8 cmd, const QByteArray &data)
{
    if (cmd == kModeRcState) {
        auto st = decodeRcState(data);
        if (!st) {
            ++m_badFrames;
            return;
        }
        if (m_lastSeq)
            m_dropped += (st->seq - *m_lastSeq - 1) & 0xFF;
        m_lastSeq = st->seq;
        ++m_frames;
        m_state = st;
        emit stateReceived();
    } else if (cmd == kCmdAck) {
        // ACK: [cmd gốc][status][sub][value…]
        if (m_inFlight && data.size() >= 2 && static_cast<quint8>(data[0]) == m_inFlight->cmd)
            finishCurrent(static_cast<quint8>(data[1]), data.size() > 3 ? data.mid(3) : QByteArray());
    }
}

void RcLink::send(quint8 cmd, const QByteArray &data)
{
    if (m_port->isOpen())
        m_port->write(buildFrame(cmd, data));
}

void RcLink::configRead(quint8 sub, std::optional<quint8> idx, AckCallback cb)
{
    QByteArray d(1, static_cast<char>(sub));
    if (idx)
        d.append(static_cast<char>(*idx));
    enqueue({kCmdConfigRead, d, kAckTimeoutMs, std::move(cb)});
}

void RcLink::configWrite(quint8 sub, const QByteArray &payload, AckCallback cb)
{
    enqueue({kCmdConfigWrite, QByteArray(1, static_cast<char>(sub)) + payload, kAckTimeoutMs, std::move(cb)});
}

void RcLink::configSave(AckCallback cb)
{
    enqueue({kCmdConfigSave, {}, kSaveTimeoutMs, std::move(cb)});
}

void RcLink::enqueue(Request req)
{
    if (!m_port->isOpen()) {
        if (req.cb)
            req.cb(kStatusTimeout, {});
        return;
    }
    m_queue.push_back(std::move(req));
    pump();
}

void RcLink::pump()
{
    if (m_inFlight || m_queue.empty())
        return;
    m_inFlight = std::move(m_queue.front());
    m_queue.pop_front();
    send(m_inFlight->cmd, m_inFlight->data);
    m_ackTimer->start(m_inFlight->timeoutMs);
}

void RcLink::finishCurrent(int status, const QByteArray &value)
{
    if (!m_inFlight)
        return;
    m_ackTimer->stop();
    AckCallback cb = std::move(m_inFlight->cb);
    m_inFlight.reset();
    if (cb)
        cb(status, value);
    pump();
}

} // namespace gcs::rc
