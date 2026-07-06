#include "app/controller.h"

#include "mavlink/mavlink_link.h"

namespace gcs::app {

using domain::Severity;
using domain::StatusText;
using domain::nowMs;

GcsController::GcsController(domain::Role role)
    : m_role(role),
      m_authority(domain::Authority::of(role)),
      m_linkManager(&m_store, [this](const StatusText &st) { postNotice(st); }),
      m_guardedSink(
          [this] { return m_linkManager.commandSink(); },
          m_authority,
          [this] {
              postNotice(StatusText(Severity::Warning,
                  QStringLiteral("Thiếu quyền điều khiển — vai trò: %1")
                      .arg(domain::roleLabel(m_role)),
                  nowMs(), true));
          })
{
    // Mọi đường lệnh đi qua sink đã gác quyền (commandSink()), không phải link
    // thô — nhờ vậy phân quyền được thực thi ở đúng một chỗ.
    m_commands = std::make_unique<mavlink::CommandService>(
        [this] { return commandSink(); },
        [this] { return m_store.snapshot(); },
        [this](const StatusText &st) { postNotice(st); });
    m_mission = std::make_unique<mavlink::MissionService>(
        [this] { return commandSink(); },
        [this](const StatusText &st) { postNotice(st); });
    // nạp giao thức nhiệm vụ mỗi tin đến (MISSION_REQUEST/ACK)
    m_linkManager.setObserver([this](const MavMessage &msg) { m_mission->onMessage(msg); });
}

void GcsController::connect(const QString &connectionString, int baud, const QString &label)
{
    disconnect();
    m_store.reset();
    postNotice(StatusText(Severity::Notice,
        QStringLiteral("Đang kết nối tới %1…").arg(label.isEmpty() ? connectionString : label),
        nowMs(), true));
    m_linkManager.start(std::make_unique<mavlink::MavlinkLink>(connectionString, baud, label));
}

void GcsController::setBridgeMode(bool enabled, int port)
{
    m_linkManager.setBridge(enabled, port, m_authority.canControl);
}

void GcsController::disconnect()
{
    if (m_linkManager.link() != nullptr)
        postNotice(StatusText(Severity::Notice, QStringLiteral("Đã ngắt kết nối"), nowMs(), true));
    m_linkManager.stop();
}

std::vector<StatusText> GcsController::drainNotices()
{
    std::vector<StatusText> out;
    std::lock_guard<std::mutex> lock(m_noticesMutex);
    while (!m_notices.empty()) {
        out.push_back(m_notices.front());
        m_notices.pop_front();
    }
    return out;
}

void GcsController::postNotice(const StatusText &st)
{
    std::lock_guard<std::mutex> lock(m_noticesMutex);
    m_notices.push_back(st);
}

} // namespace gcs::app
