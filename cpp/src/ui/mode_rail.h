// Thanh hành động/chế độ bay bên trái — dải nút dọc trên fly-view.
//
// Phản chiếu bố cục kiểu trạm mặt đất NASA: một chồng nút to (icon+nhãn) dọc mép
// trái cho các hành động thường dùng khi bay (HẠ CÁNH, VỀ, DỪNG) cộng nút
// HÀNH ĐỘNG (cất cánh). Các nút chế độ ánh xạ tới chế độ bay ArduCopter và sáng
// lên khi chế độ đó hoạt động (đọc từ HEARTBEAT).
#pragma once

#include "domain/telemetry.h"

#include <QFrame>
#include <QIcon>
#include <QMap>

class QToolButton;

namespace gcs::ui {

class ModeRail : public QFrame {
    Q_OBJECT
public:
    explicit ModeRail(QWidget *parent = nullptr);
    void setConnected(bool connected);
    void updateFrom(const domain::TelemetrySnapshot &s);

signals:
    void modeRequested(const QString &modeName);
    void takeoffRequested();

private:
    // Nút biểu tượng-trên-nhãn.
    QToolButton *makeButton(const QIcon &icon, const QString &label);

    bool m_connected = false;
    QMap<QString, QToolButton *> m_modeButtons;
    QToolButton *m_actionBtn = nullptr;
};

} // namespace gcs::ui
