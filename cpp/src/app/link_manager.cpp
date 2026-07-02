#include "app/link_manager.h"

#include "app/store.h"

namespace gcs::app {

using domain::Severity;
using domain::StatusText;
using domain::TelemetrySnapshot;
using domain::nowMs;

namespace {
constexpr int64_t kLinkTimeoutMs = 3000;    // không heartbeat lâu thế này ⇒ link down
constexpr int64_t kHeartbeatMs = 1000;      // nhịp heartbeat GCS của ta
constexpr double kRecvTimeoutS = 0.2;       // độ mịn poll recv (giữ stop() nhạy)
constexpr int64_t kStreamRequestMs = 5000;  // xin lại phương tiện phát luồng
}

LinkManager::LinkManager(TelemetryStore *store, NoticeSink noticeSink)
    : m_store(store), m_notice(std::move(noticeSink)),
      m_decoder(store, m_notice)
{
}

LinkManager::~LinkManager()
{
    stop();
}

void LinkManager::setObserver(MessageObserver observer)
{
    m_observer = std::move(observer);
}

interfaces::ICommandSink *LinkManager::commandSink() const
{
    return dynamic_cast<interfaces::ICommandSink *>(m_link.get());
}

void LinkManager::start(std::unique_ptr<interfaces::ITelemetryLink> link)
{
    stop();
    m_link = std::move(link);
    m_stop.store(false);
    m_thread = std::thread([this] { run(); });
}

void LinkManager::stop()
{
    m_stop.store(true);
    if (m_thread.joinable())
        m_thread.join();
    if (m_link) {
        m_link->close();
        m_link.reset();
    }
    if (m_store)
        m_store->mutate([](TelemetrySnapshot &s) { s.link.linkUp = false; });
}

void LinkManager::run()
{
    interfaces::ITelemetryLink *link = m_link.get();
    if (!link)
        return;
    interfaces::ICommandSink *sink = commandSink(); // cùng đối tượng, phía ghi
    try {
        link->open();
    } catch (const std::exception &e) {
        m_notice(StatusText(Severity::Error,
            QStringLiteral("Mở %1 thất bại: %2").arg(link->sourceName(), QString::fromUtf8(e.what())),
            nowMs(), true));
        return;
    }
    m_notice(StatusText(Severity::Notice,
        QStringLiteral("Đã mở link: %1").arg(link->sourceName()), nowMs(), true));

    uint64_t frames = 0, goodBytes = 0, errors = 0;
    int64_t lastHbSent = 0, lastStreamReq = 0, lastFrameMs = 0;
    bool announced = false;
    m_store->mutate([&](TelemetrySnapshot &s) { s.link.sourceName = link->sourceName(); });

    while (!m_stop.load()) {
        int64_t now = nowMs();
        if (sink && now - lastHbSent >= kHeartbeatMs) {
            try { sink->sendHeartbeat(); } catch (...) {}
            lastHbSent = now;
        }

        // Khi đã biết phương tiện, (lại) xin luồng telemetry để autopilot thật
        // gửi ATTITUDE/vị trí/trạng thái — không có bước này chân trời HUD đứng
        // im dù link đang "up".
        if (link->target().first != 0 && now - lastStreamReq >= kStreamRequestMs) {
            try { link->requestDataStreams(); } catch (...) {}
            lastStreamReq = now;
        }

        std::optional<MavMessage> msg;
        try {
            msg = link->recv(kRecvTimeoutS);
        } catch (const std::exception &e) {
            m_notice(StatusText(Severity::Error,
                QStringLiteral("Lỗi link: ") + QString::fromUtf8(e.what()), now, true));
            break;
        }

        now = nowMs();
        if (msg) {
            ++frames;
            lastFrameMs = now;
            goodBytes += msg->len + MAVLINK_NUM_NON_PAYLOAD_BYTES
                + ((msg->incompat_flags & MAVLINK_IFLAG_SIGNED) ? MAVLINK_SIGNATURE_BLOCK_LEN : 0);
            try {
                m_decoder.handle(*msg);
            } catch (...) {
                ++errors;
            }
            if (m_observer) {
                try { m_observer(*msg); } catch (...) {}
            }
            if (!announced && link->target().first != 0) {
                announced = true;
                m_notice(StatusText(Severity::Notice,
                    QStringLiteral("Phát hiện phương tiện: hệ thống %1").arg(link->target().first),
                    now, true));
            }
        }
        writeStats(frames, goodBytes, errors, lastFrameMs, now);
    }

    m_store->mutate([](TelemetrySnapshot &s) { s.link.linkUp = false; });
}

void LinkManager::writeStats(uint64_t frames, uint64_t bytes, uint64_t errors,
                             int64_t lastFrameMs, int64_t now)
{
    m_store->mutate([&](TelemetrySnapshot &s) {
        s.link.framesReceived = frames;
        s.link.bytesReceived = bytes;
        s.link.parseErrors = errors;
        s.link.lastFrameMs = lastFrameMs;
        // link_up theo độ tươi của heartbeat, không phải bất kỳ khung nào.
        s.link.linkUp = s.heartbeatSeen && (now - s.lastHeartbeatMs) < kLinkTimeoutMs;
    });
}

} // namespace gcs::app
