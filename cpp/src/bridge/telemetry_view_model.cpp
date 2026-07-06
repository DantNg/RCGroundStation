#include "bridge/telemetry_view_model.h"

#include "domain/flight_modes.h"
#include "domain/telemetry.h"

#include <QTimer>

#include <cmath>

namespace gcs::bridge {

namespace {
constexpr double kRadToDeg = 180.0 / M_PI;
constexpr int64_t kHeartbeatTimeoutMs = 3000; // heartbeat coi là "sống" trong 3 s
constexpr int64_t kLinkFullMs = 1000;         // < 1 s kể từ frame cuối = 100%
constexpr int64_t kLinkZeroMs = 5000;         // > 5 s = 0%

QString formatElapsed(int64_t ms)
{
    if (ms < 0)
        ms = 0;
    const int64_t total = ms / 1000;
    const int64_t mm = total / 60;
    const int64_t ss = total % 60;
    return QStringLiteral("%1:%2")
        .arg(mm, 2, 10, QLatin1Char('0'))
        .arg(ss, 2, 10, QLatin1Char('0'));
}
} // namespace

TelemetryViewModel::TelemetryViewModel(app::GcsController *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    m_timer = new QTimer(this);
    m_timer->setInterval(33); // ~30 Hz
    QObject::connect(m_timer, &QTimer::timeout, this, &TelemetryViewModel::poll);
    m_timer->start();
    poll(); // trạng thái ban đầu
}

void TelemetryViewModel::poll()
{
    // STATUSTEXT: đẩy ra QML trước (không phụ thuộc snapshot).
    for (const auto &notice : m_controller->drainNotices())
        emit noticeReceived(static_cast<int>(notice.severity), notice.text);

    const domain::TelemetrySnapshot s = m_controller->snapshot();
    const int64_t now = domain::nowMs();

    // ── liên kết ────────────────────────────────────────────────────────────
    const bool connected = m_controller->isConnected();
    if (connected != m_connected) { m_connected = connected; emit connectedChanged(); }

    const int64_t hbAge = s.heartbeatSeen ? (now - s.lastHeartbeatMs) : kHeartbeatTimeoutMs + 1;
    const bool heartbeat = s.heartbeatSeen && hbAge < kHeartbeatTimeoutMs;
    if (heartbeat != m_heartbeat) { m_heartbeat = heartbeat; emit heartbeatChanged(); }

    if (s.link.sourceName != m_linkSource) { m_linkSource = s.link.sourceName; emit linkSourceChanged(); }

    // linkPct: suy từ độ tươi của frame cuối (100% khi < 1 s, 0% khi > 5 s).
    int linkPct = 0;
    if (connected) {
        const int64_t age = s.link.lastFrameMs > 0 ? (now - s.link.lastFrameMs) : kLinkZeroMs;
        if (age <= kLinkFullMs)
            linkPct = 100;
        else if (age >= kLinkZeroMs)
            linkPct = 0;
        else
            linkPct = static_cast<int>(100.0 * (kLinkZeroMs - age) / (kLinkZeroMs - kLinkFullMs));
    }
    if (linkPct != m_linkPct) { m_linkPct = linkPct; emit linkPctChanged(); }

    // ── chế độ / arm ────────────────────────────────────────────────────────
    const bool armed = s.mode.armed;
    if (armed != m_armed) {
        if (armed && !m_armed)
            m_armedSinceMs = now;
        m_armed = armed;
        emit armedChanged();
    }
    const QString modeLabel = domain::flight_modes::modeName(s.mode.autopilot, s.mode.customMode);
    if (modeLabel != m_modeLabel) { m_modeLabel = modeLabel; emit modeLabelChanged(); }

    const QString armElapsed = formatElapsed(m_armed ? (now - m_armedSinceMs) : 0);
    if (armElapsed != m_armElapsed) { m_armElapsed = armElapsed; emit armElapsedChanged(); }

    // ── tư thế (rad→deg) ────────────────────────────────────────────────────
    const double roll = s.attitude.roll * kRadToDeg;
    const double pitch = s.attitude.pitch * kRadToDeg;
    // Hướng: ưu tiên hướng la bàn từ GLOBAL_POSITION_INT; nhưng một số nguồn chỉ
    // gửi trường này khi đang bay tới đích (idle = 0). Khi đó lùi về yaw của
    // ATTITUDE để la bàn/HUD vẫn cập nhật thay vì đứng yên ở 0°.
    double heading = s.position.headingDeg;
    if (!(s.position.valid && heading != 0.0))
        heading = std::fmod(s.attitude.yaw * kRadToDeg + 360.0, 360.0);
    if (roll != m_roll || pitch != m_pitch || heading != m_heading) {
        m_roll = roll; m_pitch = pitch; m_heading = heading;
        emit attitudeChanged();
    }

    // ── vị trí ──────────────────────────────────────────────────────────────
    if (s.position.altRel != m_altRel || s.position.altMsl != m_altMsl
        || s.position.lat != m_lat || s.position.lon != m_lon
        || s.position.valid != m_posValid) {
        m_altRel = s.position.altRel; m_altMsl = s.position.altMsl;
        m_lat = s.position.lat; m_lon = s.position.lon;
        m_posValid = s.position.valid;
        emit positionChanged();
    }

    // ── VFR ─────────────────────────────────────────────────────────────────
    if (s.vfr.airspeed != m_airspeed || s.vfr.groundspeed != m_groundspeed) {
        m_airspeed = s.vfr.airspeed; m_groundspeed = s.vfr.groundspeed;
        emit vfrChanged();
    }

    // ── GPS ─────────────────────────────────────────────────────────────────
    const QString fixLabel = s.gps.fixLabel();
    if (s.gps.satellites != m_satellites || fixLabel != m_fixLabel) {
        m_satellites = s.gps.satellites; m_fixLabel = fixLabel;
        emit gpsChanged();
    }

    // ── pin ─────────────────────────────────────────────────────────────────
    if (s.battery.voltage != m_battVolt || s.battery.current != m_battCurrent
        || s.battery.remaining != m_battPct) {
        m_battVolt = s.battery.voltage; m_battCurrent = s.battery.current;
        m_battPct = s.battery.remaining;
        emit batteryChanged();
    }
}

} // namespace gcs::bridge
