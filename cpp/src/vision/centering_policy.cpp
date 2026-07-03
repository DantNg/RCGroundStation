#include "vision/centering_policy.h"

#include <algorithm>
#include <cmath>

namespace gcs::vision {

namespace {
// MAV_CMD_DO_MOUNT_CONTROL — điều khiển gimbal.
constexpr int kCmdMountControl = 205;
// MAV_MOUNT_MODE_MAVLINK_TARGETING — nhận góc pitch/roll/yaw qua lệnh.
constexpr float kMountModeTargeting = 2.0f;

double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

void CenteringPolicy::steer(const interfaces::Detection &target, interfaces::ICommandSink &out)
{
    // Lỗi lệch tâm khung, ∈ [-0.5, 0.5]. x dương = mục tiêu bên phải; y dương =
    // mục tiêu phía dưới.
    const double errX = target.center().x() - 0.5;
    const double errY = target.center().y() - 0.5;

    // Vùng chết: đủ gần tâm thì không chỉnh (chống rung).
    if (std::abs(errX) > m_cfg.deadzone)
        m_yaw = clampd(m_yaw + m_cfg.gainDeg * errX, -m_cfg.yawLimit, m_cfg.yawLimit);
    if (std::abs(errY) > m_cfg.deadzone)
        // Mục tiêu ở dưới → gimbal chúc xuống (pitch âm).
        m_pitch = clampd(m_pitch - m_cfg.gainDeg * errY, m_cfg.pitchMin, m_cfg.pitchMax);

    if (!m_enabled)
        return; // dry-run: đã tính góc để UI hiện, nhưng không chạm phương tiện

    // DO_MOUNT_CONTROL: p1=pitch, p2=roll, p3=yaw, p7=mode.
    out.commandLong(kCmdMountControl,
                    float(m_pitch), 0.0f, float(m_yaw),
                    0.0f, 0.0f, 0.0f, kMountModeTargeting);
}

} // namespace gcs::vision
