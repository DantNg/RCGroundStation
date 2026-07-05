#include "mavlink/command_service.h"

#include "domain/flight_modes.h"
#include "mavlink/mav_message.h"

#include <QTimer>

namespace gcs::mavlink {

using domain::Severity;
using domain::StatusText;
using domain::nowMs;
namespace fm = domain::flight_modes;

namespace {
constexpr int kCustomModeEnabled = 1; // MAV_MODE_FLAG_CUSTOM_MODE_ENABLED
constexpr float kArmMagicForce = 21196.0f; // param2 buộc (dis)arm bỏ qua kiểm tra
// Chờ phương tiện vào GUIDED trước khi cất cánh. Nếu gửi TAKEOFF ngay sau lệnh
// đổi chế độ (trong cùng chu kỳ xả outbox), autopilot nhận TAKEOFF trước khi kịp
// vào GUIDED và xác nhận vị trí → trả về "Need position estimate". Mission Planner
// không dính lỗi này vì có độ trễ thao tác tay giữa đổi mode và takeoff.
constexpr int kGuidedPollMs = 250;       // nhịp kiểm tra heartbeat báo GUIDED
constexpr int64_t kGuidedWaitMs = 3000;  // tối đa chờ vào GUIDED rồi bỏ cuộc
}

CommandService::CommandService(LinkProvider link, StateProvider state, NoticeSink notify)
    : m_link(std::move(link)), m_state(std::move(state)), m_notify(std::move(notify))
{
}

void CommandService::arm(bool force) { sendArm(true, force); }
void CommandService::disarm(bool force) { sendArm(false, force); }

void CommandService::sendArm(bool arm, bool force)
{
    auto *sink = requireLink();
    if (!sink)
        return;
    const QString verb = arm ? QStringLiteral("ARM") : QStringLiteral("DISARM");
    try {
        sink->commandLong(MAV_CMD_COMPONENT_ARM_DISARM,
                          arm ? 1.0f : 0.0f,
                          force ? kArmMagicForce : 0.0f);
        info(verb + QStringLiteral(" đã gửi") + (force ? QStringLiteral(" (buộc)") : QString()));
    } catch (const std::exception &e) {
        error(verb + QStringLiteral(" thất bại: ") + QString::fromUtf8(e.what()));
    }
}

void CommandService::setModeByName(const QString &modeName)
{
    auto *sink = requireLink();
    if (!sink)
        return;
    switchMode(sink, modeName, true);
}

void CommandService::takeoff(double altitudeM)
{
    auto *sink = requireLink();
    if (!sink)
        return;
    if (!m_state().mode.armed) {
        error(QStringLiteral("Từ chối cất cánh: phương tiện chưa ARM — hãy arm trước"));
        return;
    }
    const auto s = m_state();
    const QString current = fm::modeName(s.mode.autopilot, s.mode.customMode);
    if (current == QLatin1String("GUIDED")) {
        sendTakeoff(altitudeM); // đã ở GUIDED → cất cánh ngay
        return;
    }
    // Chưa ở GUIDED: chuyển chế độ rồi CHỜ heartbeat xác nhận mới gửi TAKEOFF,
    // tránh lỗi "Need position estimate" do lệnh tới trước khi vào GUIDED.
    ensureGuided(sink);
    info(QStringLiteral("Chờ vào GUIDED trước khi cất cánh…"));
    waitForGuidedThenTakeoff(altitudeM, nowMs() + kGuidedWaitMs);
}

void CommandService::waitForGuidedThenTakeoff(double altitudeM, int64_t deadlineMs)
{
    QTimer::singleShot(kGuidedPollMs, [this, altitudeM, deadlineMs] {
        if (!m_link()) {
            error(QStringLiteral("Mất kết nối — hủy cất cánh"));
            return;
        }
        const auto s = m_state();
        if (!s.mode.armed) {
            error(QStringLiteral("Phương tiện đã disarm — hủy cất cánh"));
            return;
        }
        const QString current = fm::modeName(s.mode.autopilot, s.mode.customMode);
        if (current == QLatin1String("GUIDED")) {
            sendTakeoff(altitudeM);
            return;
        }
        if (nowMs() >= deadlineMs) {
            error(QStringLiteral("Không vào được GUIDED sau %1 s — hủy cất cánh. "
                                 "Kiểm tra GPS/EKF (Need position estimate).")
                      .arg(kGuidedWaitMs / 1000.0, 0, 'f', 0));
            return;
        }
        waitForGuidedThenTakeoff(altitudeM, deadlineMs); // thử lại
    });
}

void CommandService::sendTakeoff(double altitudeM)
{
    auto *sink = requireLink();
    if (!sink)
        return;
    try {
        sink->commandLong(MAV_CMD_NAV_TAKEOFF, 0, 0, 0, 0, 0, 0,
                          static_cast<float>(altitudeM));
        info(QStringLiteral("Cất cánh lên %1 m").arg(altitudeM, 0, 'f', 0));
    } catch (const std::exception &e) {
        error(QStringLiteral("Cất cánh thất bại: ") + QString::fromUtf8(e.what()));
    }
}

void CommandService::flyTo(double lat, double lon, double altRel)
{
    auto *sink = requireLink();
    if (!sink)
        return;
    if (!m_state().mode.armed)
        m_notify(StatusText(Severity::Warning,
            QStringLiteral("Gửi bay-đến khi chưa ARM — phương tiện sẽ bỏ qua"),
            nowMs(), true));
    ensureGuided(sink);
    try {
        sink->setPositionTargetGlobal(lat, lon, altRel);
        info(QStringLiteral("Bay đến %1, %2 @ %3 m")
                 .arg(lat, 0, 'f', 6).arg(lon, 0, 'f', 6).arg(altRel, 0, 'f', 0));
    } catch (const std::exception &e) {
        error(QStringLiteral("Bay-đến thất bại: ") + QString::fromUtf8(e.what()));
    }
}

void CommandService::startMission()
{
    auto *sink = requireLink();
    if (!sink)
        return;
    if (!m_state().mode.armed)
        m_notify(StatusText(Severity::Warning,
            QStringLiteral("Bắt đầu nhiệm vụ khi chưa ARM — hãy arm trước"),
            nowMs(), true));
    switchMode(sink, QStringLiteral("AUTO"), true);
    try {
        sink->commandLong(MAV_CMD_MISSION_START, 0, 0);
        info(QStringLiteral("Bắt đầu nhiệm vụ (AUTO)"));
    } catch (const std::exception &e) {
        error(QStringLiteral("Bắt đầu nhiệm vụ thất bại: ") + QString::fromUtf8(e.what()));
    }
}

void CommandService::ensureGuided(interfaces::ICommandSink *sink)
{
    const auto s = m_state();
    const QString current = fm::modeName(s.mode.autopilot, s.mode.customMode);
    if (current != QLatin1String("GUIDED")) {
        switchMode(sink, QStringLiteral("GUIDED"), false);
        info(QStringLiteral("Chế độ → GUIDED (bắt buộc cho lệnh guided)"));
    }
}

void CommandService::switchMode(interfaces::ICommandSink *sink, const QString &modeName, bool announce)
{
    const int autopilot = m_state().mode.autopilot;
    const auto customMode = fm::modeId(autopilot, modeName);
    if (!customMode) {
        error(QStringLiteral("Chế độ không xác định '%1' cho autopilot này").arg(modeName));
        return;
    }
    try {
        sink->setMode(kCustomModeEnabled, *customMode);
        if (announce)
            info(QStringLiteral("Chế độ → %1").arg(modeName));
    } catch (const std::exception &e) {
        error(QStringLiteral("Đặt chế độ %1 thất bại: %2").arg(modeName, QString::fromUtf8(e.what())));
    }
}

interfaces::ICommandSink *CommandService::requireLink()
{
    auto *sink = m_link();
    if (!sink)
        error(QStringLiteral("Chưa kết nối — bỏ qua lệnh"));
    return sink;
}

void CommandService::info(const QString &text)
{
    m_notify(StatusText(Severity::Notice, text, nowMs(), true));
}

void CommandService::error(const QString &text)
{
    m_notify(StatusText(Severity::Error, text, nowMs(), true));
}

} // namespace gcs::mavlink
