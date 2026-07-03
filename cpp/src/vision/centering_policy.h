// Chính sách bắt bám mặc định: lái GIMBAL để giữ mục tiêu ở tâm khung.
//
// Chọn lái gimbal (không lái phương tiện) vì đây là mặc định AN TOÀN — camera
// quay theo người, drone không tự bay. Bộ điều khiển tỉ lệ tích luỹ trên lỗi
// lệch tâm (chuẩn hoá), phát MAV_CMD_DO_MOUNT_CONTROL. Lệnh chỉ thực sự gửi khi
// được bật tường minh; ở chế độ dry-run vẫn tính góc để UI hiển thị.
#pragma once

#include "interfaces/vision.h"

namespace gcs::vision {

class CenteringPolicy : public interfaces::ITrackingPolicy {
public:
    struct Config {
        double deadzone = 0.06;   // bỏ qua lỗi nhỏ hơn (chuẩn hoá, ±)
        double gainDeg = 6.0;     // độ/khung cho mỗi đơn vị lỗi
        double yawLimit = 90.0;   // kẹp góc yaw gimbal (độ)
        double pitchMin = -90.0;  // nhìn thẳng xuống
        double pitchMax = 20.0;   // hơi ngước lên
    };

    CenteringPolicy() = default;
    explicit CenteringPolicy(Config cfg) : m_cfg(cfg) {}

    void steer(const interfaces::Detection &target, interfaces::ICommandSink &out) override;
    void onLost() override {}   // giữ nguyên góc gimbal khi mất mục tiêu

    // Bật/tắt việc THỰC SỰ gửi lệnh tới phương tiện (mặc định tắt = dry-run).
    void setEnabled(bool on) { m_enabled = on; }
    bool isEnabled() const { return m_enabled; }

    double yawDeg() const { return m_yaw; }
    double pitchDeg() const { return m_pitch; }

private:
    Config m_cfg;
    bool m_enabled = false;
    double m_yaw = 0.0;
    double m_pitch = 0.0;
};

} // namespace gcs::vision
