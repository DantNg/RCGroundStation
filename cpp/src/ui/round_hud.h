// Đồng hồ tư thế tròn nhỏ gọn (một "quả bóng" PFD).
//
// Thiết kế để nổi trên bản đồ trong fly-view tự hành: nhỏ, liếc là thấy, tự
// chứa. Nó cắt chân trời gradient vào một hình tròn, bọc trong vành có thang
// nghiêng, và chỉ hiện bức tranh IMU/tư thế — roll & pitch từ quả bóng, yaw
// trong ô heading, cùng một chỉ số độ cao bên dưới.
#pragma once

#include "domain/telemetry.h"

#include <QWidget>

namespace gcs::ui {

class RoundHud : public QWidget {
    Q_OBJECT
public:
    explicit RoundHud(QWidget *parent = nullptr);
    void updateFrom(const domain::TelemetrySnapshot &s);

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    void drawBall(QPainter &p, double cx, double cy, double R, double s);
    void drawBezelAndRoll(QPainter &p, double cx, double cy, double R, double s);
    void drawBoresight(QPainter &p, double cx, double cy, double s);
    void drawHeadingBox(QPainter &p, double cx, double cy, double R, double s);
    void drawAlt(QPainter &p, int w, double y, double stripH, double s);
    void drawNoData(QPainter &p, double cx, double cy, double R, double s);

    double m_roll = 0, m_pitch = 0, m_heading = 0;
    double m_alt = 0, m_airspeed = 0, m_climb = 0;
    bool m_valid = false;
    QString m_mode = QStringLiteral("—");
    bool m_armed = false;
    double m_battV = 0;
    int m_battPct = -1;
    int m_gpsFix = 0, m_gpsSats = 0;
    bool m_linkUp = false;
};

} // namespace gcs::ui
