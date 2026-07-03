// Cửa sổ chính — fly view bản đồ/camera với bố cục thiết bị kiểu NASA.
//
// Không gian làm việc (bản đồ vệ tinh hoặc camera) phủ đầy cửa sổ. Nổi lên trên:
// một thanh trạng thái toàn chiều rộng, một thanh hành động bên trái, một HUD tư
// thế tròn nhỏ, ô picture-in-picture của camera, nhật ký, biểu ngữ cảnh báo, và
// một dock góc chứa điều khiển riêng của view đang hoạt động.
//
// Đây là đối tượng duy nhất biết về GcsController; một bộ đếm ~25 Hz đọc kho,
// nạp các overlay thụ động và rút thông báo vào nhật ký + biểu ngữ cảnh báo.
#pragma once

#include "config.h"

#include <QWidget>

class QTimer;

namespace gcs::app { class GcsController; }

namespace gcs::ui {

class MapWidget;
class CameraView;
class RoundHud;
class MessagesPanel;
class WarningOverlay;
class StatusBar;
class ModeRail;
class ControlDock;
class PipOverlay;
class ConnectionBar;
class OverlayStage;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    MainWindow(app::GcsController *controller, AppConfig config);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    QWidget *view(const QString &name);
    QString pipName() const;
    void applyViewRoles();
    void swapViews();
    void relayout();
    void layoutOverlays(int w, int h);

    void onArm(bool force);
    void onTakeoff();
    void onStartMission();
    bool confirm(const QString &title, const QString &text);
    void onConnect(const AppConfig &cfg);
    void onDisconnect();
    void setConnected(bool connected);
    void toggleFullscreen();
    void exitFullscreen();
    void tick();

    app::GcsController *m_controller;
    AppConfig m_config;
    bool m_connected = false;
    bool m_showMessages = false;   // panel nhật ký tắt mặc định (phím M để bật)
    QString m_primary = QStringLiteral("map");
    double m_lastTakeoffAlt = 10.0;

    ConnectionBar *m_connBar = nullptr;
    MapWidget *m_map = nullptr;
    CameraView *m_camera = nullptr;
    RoundHud *m_hud = nullptr;
    MessagesPanel *m_messages = nullptr;
    WarningOverlay *m_warnings = nullptr;
    StatusBar *m_status = nullptr;
    ModeRail *m_rail = nullptr;
    ControlDock *m_dock = nullptr;
    PipOverlay *m_pipFrame = nullptr;
    OverlayStage *m_stage = nullptr;
    QTimer *m_timer = nullptr;
};

} // namespace gcs::ui
