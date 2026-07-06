#include "interfaces/authority_sink.h"

#include "domain/telemetry.h"

#include <utility>

namespace gcs::interfaces {

AuthorityGuardedSink::AuthorityGuardedSink(Provider inner, domain::Authority auth,
                                           DenyNotice onDeny)
    : m_inner(std::move(inner)), m_auth(auth), m_onDeny(std::move(onDeny))
{
}

ICommandSink *AuthorityGuardedSink::pass()
{
    if (m_auth.canControl)
        return m_inner ? m_inner() : nullptr;

    // Bị chặn vì thiếu quyền — báo có tiết chế (tránh spam ở lệnh tần suất cao).
    const int64_t now = domain::nowMs();
    if (m_onDeny && now - m_lastDenyMs > 3000) {
        m_lastDenyMs = now;
        m_onDeny();
    }
    return nullptr;
}

void AuthorityGuardedSink::commandLong(int command, float p1, float p2, float p3, float p4,
                                       float p5, float p6, float p7, int confirmation)
{
    if (auto *s = pass())
        s->commandLong(command, p1, p2, p3, p4, p5, p6, p7, confirmation);
}

void AuthorityGuardedSink::setMode(int baseMode, int customMode)
{
    if (auto *s = pass())
        s->setMode(baseMode, customMode);
}

void AuthorityGuardedSink::setPositionTargetGlobal(double lat, double lon, double altRel)
{
    if (auto *s = pass())
        s->setPositionTargetGlobal(lat, lon, altRel);
}

void AuthorityGuardedSink::sendHeartbeat()
{
    // Heartbeat GCS là "tuyên bố quyền điều khiển" — chỉ cấp được điều khiển mới
    // phát. Chặn im lặng (không báo) để khỏi nhiễu ở ~1 Hz.
    if (!m_auth.canControl)
        return;
    if (ICommandSink *s = m_inner ? m_inner() : nullptr)
        s->sendHeartbeat();
}

void AuthorityGuardedSink::missionCount(int count, int missionType)
{
    if (auto *s = pass())
        s->missionCount(count, missionType);
}

void AuthorityGuardedSink::missionItemInt(const MissionItem &item)
{
    if (auto *s = pass())
        s->missionItemInt(item);
}

void AuthorityGuardedSink::sendRawFrame(const QByteArray &frame)
{
    // Khung thô từ GCS ngoài (cầu nối) vẫn phải qua cửa quyền: chỉ cấp được điều
    // khiển mới cho tiêm về phương tiện.
    if (auto *s = pass())
        s->sendRawFrame(frame);
}

} // namespace gcs::interfaces
