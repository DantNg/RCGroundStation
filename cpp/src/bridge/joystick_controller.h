// Lớp cầu nối cần lái ảo (joystick) cho QML.
//
// Phơi ra một "cần lái mềm": QML đọc/ghi trục chuẩn hoá, còn lớp này bơm
// MANUAL_CONTROL tới phương tiện ở tần suất đều (~25 Hz) khi được BẬT. Có thể
// bật/tắt bất cứ lúc nào (thuộc tính ``enabled``) — khi tắt, luồng dừng và các
// trục về giữa. Mọi khung đi qua ``controller.commandSink()`` (đã gác quyền), nên
// bản dựng chỉ-xem không thể điều khiển dù giao diện có lộ nút.
//
// Quy ước trục (đầu vào từ QML, chuẩn hoá):
//   roll, pitch, yaw ∈ [-1, 1]   (0 = giữa)
//   throttle        ∈ [ 0, 1]    (0.5 = giữa/hover)
// Được ánh xạ sang thang MAVLink của MANUAL_CONTROL trong tick().
#pragma once

#include "app/controller.h"

#include <QObject>
#include <QTimer>

namespace gcs::bridge {

class JoystickController : public QObject {
    Q_OBJECT
    // Bật/tắt cần lái ảo. Khi bật, bắt đầu bơm MANUAL_CONTROL đều đặn.
    Q_PROPERTY(bool enabled READ enabled WRITE setEnabled NOTIFY enabledChanged)
    // Tần suất phát khung (Hz) — để QML hiển thị nếu cần.
    Q_PROPERTY(int rateHz READ rateHz CONSTANT)

public:
    explicit JoystickController(app::GcsController *controller, QObject *parent = nullptr);

    bool enabled() const { return m_enabled; }
    void setEnabled(bool on);
    int rateHz() const { return kRateHz; }

    // Đặt vị trí cần lái từ QML (chuẩn hoá). Được lưu và gửi ở tick kế tiếp.
    Q_INVOKABLE void setAxes(qreal roll, qreal pitch, qreal yaw, qreal throttle);
    // Đặt bitmask nút (tuỳ chọn) — vd nút hành động trên cần lái.
    Q_INVOKABLE void setButtons(int mask);

signals:
    void enabledChanged();

private:
    static constexpr int kRateHz = 25;

    void tick();       // gửi một khung MANUAL_CONTROL theo trục hiện tại
    void sendFrame();  // đóng gói + gửi qua command sink đã gác quyền

    app::GcsController *m_controller;
    QTimer m_timer;
    bool m_enabled = false;

    // trục chuẩn hoá hiện tại
    qreal m_roll = 0.0;
    qreal m_pitch = 0.0;
    qreal m_yaw = 0.0;
    qreal m_throttle = 0.5; // giữa
    int m_buttons = 0;
};

} // namespace gcs::bridge
