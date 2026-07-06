// Lớp cầu nối lệnh cho QML.
//
// Bọc ``GcsController`` để QML gọi được các hành động cấp cao: mở/đóng link
// (UDP/TCP/Serial), arm/disarm, đổi chế độ, cất cánh, bay-đến, chạy nhiệm vụ.
// Mọi lệnh đi qua ``controller.commands()`` (đã gác quyền) — không tự chế MAVLink.
//
// ``quickModes`` là danh sách nút chế độ (label/mode/desc) cho ModeRail.
#pragma once

#include "app/controller.h"
#include "config.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVariantList>

namespace gcs::bridge {

class Backend : public QObject {
    Q_OBJECT
    // Danh sách chế độ nhanh: mỗi phần tử là { label, mode, desc }.
    Q_PROPERTY(QVariantList quickModes READ quickModes CONSTANT)
    // Chế độ cầu nối MAVLink (chuyển tiếp qua Wi-Fi cho máy tính đọc).
    Q_PROPERTY(bool bridgeEnabled READ bridgeEnabled NOTIFY bridgeChanged)
    Q_PROPERTY(int bridgePort READ bridgePort NOTIFY bridgeChanged)

public:
    explicit Backend(app::GcsController *controller, AppConfig config, QObject *parent = nullptr);

    QVariantList quickModes() const;
    bool bridgeEnabled() const { return m_config.bridgeEnabled; }
    int bridgePort() const { return m_config.bridgePort; }

    // ── liên kết ────────────────────────────────────────────────────────────
    Q_INVOKABLE QStringList serialPorts() const;
    Q_INVOKABLE void connectUdp(int port);
    Q_INVOKABLE void connectTcp(const QString &host, int port);
    Q_INVOKABLE void connectSerial(const QString &portName, int baud);
    Q_INVOKABLE void disconnect();

    // ── lệnh ────────────────────────────────────────────────────────────────
    Q_INVOKABLE void arm(bool force = false);
    Q_INVOKABLE void disarm(bool force = false);
    Q_INVOKABLE void setMode(const QString &modeName);
    Q_INVOKABLE void takeoff(double altitudeM);
    Q_INVOKABLE void flyTo(double lat, double lon, double altRel);
    Q_INVOKABLE void startMission();

    // ── chế độ cầu nối ────────────────────────────────────────────────────────
    // Bật/tắt và đặt cổng broadcast. Lưu vào cấu hình và áp dụng ngay (nếu đang
    // kết nối) lẫn cho các lần kết nối sau.
    Q_INVOKABLE void setBridgeMode(bool enabled, int port);

    // Chia sẻ điểm waypoint đang chọn trên bản đồ ra mạng cầu nối (trạm giám sát
    // sẽ thấy marker nhấp nháy). ``clearSharedWaypoint`` khi huỷ chọn.
    Q_INVOKABLE void shareWaypoint(double lat, double lon, double altRel);
    Q_INVOKABLE void clearSharedWaypoint();

signals:
    void bridgeChanged();

private:
    void openWith(AppConfig cfg);

    app::GcsController *m_controller;
    AppConfig m_config;
};

} // namespace gcs::bridge
