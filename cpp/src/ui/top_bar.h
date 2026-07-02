// Thanh trạng thái trên toàn chiều rộng + dock nhỏ cho điều khiển view hiện hành.
//
// Mép trên fly-view là một thanh kính mờ (StatusBar) phản chiếu một trạm mặt đất
// hiện đại: tên phương tiện bên trái, và bên phải là các chip pin / GPS / link
// cạnh viên chế độ bay (nhấp để chọn) và viên arm (nhấp để arm/disarm).
//
// Điều khiển riêng của mỗi view (dải bản đồ hoặc dải camera) nằm trong một
// ControlDock nổi ở góc.
#pragma once

#include "domain/telemetry.h"
#include "ui/acrylic.h"

class QLabel;
class QPushButton;
class QHBoxLayout;

namespace gcs::ui {

class StatusBar : public acrylic::AcrylicFrame {
    Q_OBJECT
public:
    explicit StatusBar(QWidget *parent = nullptr);

    void setCompact(bool compact);
    void setLabel(const QString &label);
    void setConnected(bool connected);
    void updateFrom(const domain::TelemetrySnapshot &s);

signals:
    void armRequested(bool force);
    void disarmRequested(bool force);
    void modeRequested(const QString &modeName);
    void disconnectRequested();

private:
    QLabel *chip(const QString &text);
    void recolor(QLabel *lbl, const QString &text, const QString &color);
    void setArmed(bool armed);
    void onArmClicked();

    bool m_connected = false;
    bool m_armed = false;
    QLabel *m_dot = nullptr;
    QLabel *m_vehicle = nullptr;
    QLabel *m_tag = nullptr;
    QPushButton *m_modePill = nullptr;
    QPushButton *m_armPill = nullptr;
    QLabel *m_gps = nullptr;
    QLabel *m_link = nullptr;
    QLabel *m_batt = nullptr;
    QPushButton *m_disc = nullptr;
};

class ControlDock : public acrylic::AcrylicFrame {
    Q_OBJECT
public:
    explicit ControlDock(QWidget *parent = nullptr);
    void attachControls(QWidget *mapCtrl, QWidget *camCtrl);
    void setActive(const QString &which); // "map" | "cam"

private:
    QWidget *m_mapCtrl = nullptr;
    QWidget *m_camCtrl = nullptr;
    QHBoxLayout *m_row = nullptr;
};

} // namespace gcs::ui
