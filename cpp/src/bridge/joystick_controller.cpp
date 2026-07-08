#include "bridge/joystick_controller.h"

#include <algorithm>
#include <cmath>

namespace gcs::bridge {

namespace {
// Giới hạn [-1, 1] rồi đổi sang thang MAVLink [-1000, 1000].
int16_t axisToMav(qreal v)
{
    v = std::clamp(v, -1.0, 1.0);
    return static_cast<int16_t>(std::lround(v * 1000.0));
}
// Ga [0, 1] → [0, 1000] (500 = giữa/hover).
int16_t throttleToMav(qreal v)
{
    v = std::clamp(v, 0.0, 1.0);
    return static_cast<int16_t>(std::lround(v * 1000.0));
}
} // namespace

JoystickController::JoystickController(app::GcsController *controller, QObject *parent)
    : QObject(parent), m_controller(controller)
{
    m_timer.setInterval(1000 / kRateHz);
    m_timer.setTimerType(Qt::PreciseTimer);
    connect(&m_timer, &QTimer::timeout, this, &JoystickController::tick);
}

void JoystickController::setEnabled(bool on)
{
    if (m_enabled == on)
        return;
    m_enabled = on;
    if (m_enabled) {
        m_timer.start();
    } else {
        m_timer.stop();
        // Về giữa: cần lái nhả tay, không giữ lệnh cũ. Gửi một khung trung tính
        // cuối để phương tiện thấy cần đã ở giữa trước khi luồng dừng hẳn.
        m_roll = m_pitch = m_yaw = 0.0;
        m_throttle = 0.5;
        m_buttons = 0;
        sendFrame();
    }
    emit enabledChanged();
}

void JoystickController::setAxes(qreal roll, qreal pitch, qreal yaw, qreal throttle)
{
    m_roll = roll;
    m_pitch = pitch;
    m_yaw = yaw;
    m_throttle = throttle;
}

void JoystickController::setButtons(int mask)
{
    m_buttons = mask;
}

void JoystickController::tick()
{
    if (!m_enabled)
        return;
    sendFrame();
}

void JoystickController::sendFrame()
{
    // commandSink() là nullptr khi chưa kết nối, và đã gác quyền: bản dựng chỉ-xem
    // sẽ bị chặn im lặng bên trong. manualControl() cũng tự bỏ khung nếu chưa có
    // heartbeat của phương tiện.
    interfaces::ICommandSink *sink = m_controller->commandSink();
    if (!sink)
        return;
    sink->manualControl(axisToMav(m_pitch), axisToMav(m_roll),
                        throttleToMav(m_throttle), axisToMav(m_yaw),
                        m_buttons);
}

} // namespace gcs::bridge
