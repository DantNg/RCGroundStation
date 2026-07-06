// Lớp cầu nối telemetry cho QML.
//
// Bao quanh ``GcsController``: định kỳ (QTimer ~30 Hz) chụp ``snapshot()``, rút
// gọn thành các Q_PROPERTY phẳng để QML bind (telemetry.roll, telemetry.battPct…)
// và chỉ phát tín hiệu *Changed khi giá trị thực sự đổi (dirty-check) để không
// ép QML vẽ lại vô ích. STATUSTEXT được đẩy ra ngoài qua noticeReceived().
//
// Không có logic nghiệp vụ ở đây — chỉ chuyển đổi đơn vị/định dạng cho hiển thị.
#pragma once

#include "app/controller.h"

#include <QObject>
#include <QString>
#include <QtGlobal>

class QTimer;

namespace gcs::bridge {

class TelemetryViewModel : public QObject {
    Q_OBJECT

    // ── liên kết / heartbeat ────────────────────────────────────────────────
    Q_PROPERTY(bool connected READ connected NOTIFY connectedChanged)
    Q_PROPERTY(bool heartbeat READ heartbeat NOTIFY heartbeatChanged)
    Q_PROPERTY(QString linkSource READ linkSource NOTIFY linkSourceChanged)
    Q_PROPERTY(int linkPct READ linkPct NOTIFY linkPctChanged)

    // ── chế độ / arm ────────────────────────────────────────────────────────
    Q_PROPERTY(bool armed READ armed NOTIFY armedChanged)
    Q_PROPERTY(QString armLabel READ armLabel NOTIFY armedChanged)
    Q_PROPERTY(QString modeLabel READ modeLabel NOTIFY modeLabelChanged)
    Q_PROPERTY(QString armElapsed READ armElapsed NOTIFY armElapsedChanged)

    // ── tư thế ──────────────────────────────────────────────────────────────
    Q_PROPERTY(double roll READ roll NOTIFY attitudeChanged)
    Q_PROPERTY(double pitch READ pitch NOTIFY attitudeChanged)
    Q_PROPERTY(double heading READ heading NOTIFY attitudeChanged)

    // ── vị trí ──────────────────────────────────────────────────────────────
    Q_PROPERTY(double altRel READ altRel NOTIFY positionChanged)
    Q_PROPERTY(double altMsl READ altMsl NOTIFY positionChanged)
    Q_PROPERTY(double lat READ lat NOTIFY positionChanged)
    Q_PROPERTY(double lon READ lon NOTIFY positionChanged)
    Q_PROPERTY(bool posValid READ posValid NOTIFY positionChanged)

    // ── waypoint chia sẻ qua cầu nối (trạm giám sát vẽ marker nhấp nháy) ──────
    Q_PROPERTY(bool sharedWpValid READ sharedWpValid NOTIFY sharedWaypointChanged)
    Q_PROPERTY(double sharedWpLat READ sharedWpLat NOTIFY sharedWaypointChanged)
    Q_PROPERTY(double sharedWpLon READ sharedWpLon NOTIFY sharedWaypointChanged)

    // ── khí động (VFR) ──────────────────────────────────────────────────────
    Q_PROPERTY(double airspeed READ airspeed NOTIFY vfrChanged)
    Q_PROPERTY(double groundspeed READ groundspeed NOTIFY vfrChanged)

    // ── GPS ─────────────────────────────────────────────────────────────────
    Q_PROPERTY(int satellites READ satellites NOTIFY gpsChanged)
    Q_PROPERTY(QString fixLabel READ fixLabel NOTIFY gpsChanged)

    // ── pin ─────────────────────────────────────────────────────────────────
    Q_PROPERTY(double battVolt READ battVolt NOTIFY batteryChanged)
    Q_PROPERTY(double battCurrent READ battCurrent NOTIFY batteryChanged)
    Q_PROPERTY(int battPct READ battPct NOTIFY batteryChanged)

public:
    explicit TelemetryViewModel(app::GcsController *controller, QObject *parent = nullptr);

    bool connected() const { return m_connected; }
    bool heartbeat() const { return m_heartbeat; }
    QString linkSource() const { return m_linkSource; }
    int linkPct() const { return m_linkPct; }

    bool armed() const { return m_armed; }
    QString armLabel() const { return m_armed ? QStringLiteral("● ĐÃ ARM") : QStringLiteral("○ CHƯA ARM"); }
    QString modeLabel() const { return m_modeLabel; }
    QString armElapsed() const { return m_armElapsed; }

    double roll() const { return m_roll; }
    double pitch() const { return m_pitch; }
    double heading() const { return m_heading; }

    double altRel() const { return m_altRel; }
    double altMsl() const { return m_altMsl; }
    double lat() const { return m_lat; }
    double lon() const { return m_lon; }
    bool posValid() const { return m_posValid; }

    bool sharedWpValid() const { return m_sharedWpValid; }
    double sharedWpLat() const { return m_sharedWpLat; }
    double sharedWpLon() const { return m_sharedWpLon; }

    double airspeed() const { return m_airspeed; }
    double groundspeed() const { return m_groundspeed; }

    int satellites() const { return m_satellites; }
    QString fixLabel() const { return m_fixLabel; }

    double battVolt() const { return m_battVolt; }
    double battCurrent() const { return m_battCurrent; }
    int battPct() const { return m_battPct; }

signals:
    void connectedChanged();
    void heartbeatChanged();
    void linkSourceChanged();
    void linkPctChanged();
    void armedChanged();
    void modeLabelChanged();
    void armElapsedChanged();
    void attitudeChanged();
    void positionChanged();
    void sharedWaypointChanged();
    void vfrChanged();
    void gpsChanged();
    void batteryChanged();
    // severity theo domain::Severity (0 = nặng nhất); text đã sẵn hiển thị.
    void noticeReceived(int severity, const QString &text);

private:
    void poll();

    app::GcsController *m_controller;
    QTimer *m_timer;

    // đồng hồ arm — ghi mốc lúc chuyển disarmed→armed
    int64_t m_armedSinceMs = 0;

    // trạng thái đã cache (nguồn của mọi getter)
    bool m_connected = false;
    bool m_heartbeat = false;
    QString m_linkSource = QStringLiteral("—");
    int m_linkPct = 0;
    bool m_armed = false;
    QString m_modeLabel = QStringLiteral("—");
    QString m_armElapsed = QStringLiteral("00:00");
    double m_roll = 0.0, m_pitch = 0.0, m_heading = 0.0;
    double m_altRel = 0.0, m_altMsl = 0.0, m_lat = 0.0, m_lon = 0.0;
    bool m_posValid = false;
    bool m_sharedWpValid = false;
    double m_sharedWpLat = 0.0, m_sharedWpLon = 0.0;
    double m_airspeed = 0.0, m_groundspeed = 0.0;
    int m_satellites = 0;
    QString m_fixLabel = QStringLiteral("Không GPS");
    double m_battVolt = 0.0, m_battCurrent = 0.0;
    int m_battPct = -1;
};

} // namespace gcs::bridge
