#include "ui/main_window.h"

#include "app/controller.h"
#include "ui/acrylic.h"
#include "ui/camera_view.h"
#include "ui/connection_bar.h"
#include "ui/map_widget.h"
#include "ui/messages_panel.h"
#include "ui/mode_rail.h"
#include "ui/overlay_stage.h"
#include "ui/round_hud.h"
#include "ui/theme.h"
#include "ui/top_bar.h"
#include "ui/warning_overlay.h"
#include "ui/widgets.h"

#include <QInputDialog>
#include <QMessageBox>
#include <QShortcut>
#include <QTimer>
#include <QVBoxLayout>

#include <algorithm>

namespace gcs::ui {

namespace {
constexpr int kRefreshHz = 25;
double clampd(double v, double lo, double hi) { return v < lo ? lo : (v > hi ? hi : v); }
}

MainWindow::MainWindow(app::GcsController *controller, AppConfig config)
    : m_controller(controller), m_config(std::move(config))
{
    setWindowTitle(QStringLiteral("Trạm Điều Khiển Mặt Đất — Desktop"));
    resize(1280, 720);
    setMinimumSize(460, 320);
    setStyleSheet(theme::stylesheet());

    // thanh kính mờ lấy mẫu view nào đang là chính
    acrylic::setBackdropProvider([this] { return view(m_primary); });

    auto *root = new QVBoxLayout(this);
    root->setContentsMargins(8, 8, 8, 8);
    root->setSpacing(6);

    m_connBar = new ConnectionBar(m_config);
    connect(m_connBar, &ConnectionBar::connectRequested, this, &MainWindow::onConnect);
    connect(m_connBar, &ConnectionBar::disconnectRequested, this, &MainWindow::onDisconnect);

    m_map = new MapWidget;
    m_camera = new CameraView;
    m_hud = new RoundHud;
    m_messages = new MessagesPanel;
    m_warnings = new WarningOverlay;
    m_status = new StatusBar;
    m_rail = new ModeRail;
    m_dock = new ControlDock;
    m_dock->attachControls(m_map->controlsWidget(), m_camera->headerWidget());
    m_pipFrame = new PipOverlay;

    // nối dây: thanh trạng thái + thanh hành động báo ý định; controller thực thi
    connect(m_status, &StatusBar::armRequested, this, &MainWindow::onArm);
    connect(m_status, &StatusBar::disarmRequested, this,
            [this](bool force) { m_controller->commands().disarm(force); });
    connect(m_status, &StatusBar::modeRequested, this,
            [this](const QString &n) { m_controller->commands().setModeByName(n); });
    connect(m_status, &StatusBar::disconnectRequested, this, &MainWindow::onDisconnect);
    connect(m_rail, &ModeRail::modeRequested, this,
            [this](const QString &n) { m_controller->commands().setModeByName(n); });
    connect(m_rail, &ModeRail::takeoffRequested, this, &MainWindow::onTakeoff);
    connect(m_map, &MapWidget::flyToRequested, this,
            [this](double lat, double lon, double alt) { m_controller->commands().flyTo(lat, lon, alt); });
    connect(m_map, &MapWidget::missionUploadRequested, this,
            [this](const std::vector<domain::Waypoint> &wps) { m_controller->mission().upload(wps); });
    connect(m_map, &MapWidget::missionStartRequested, this, &MainWindow::onStartMission);
    connect(m_pipFrame, &PipOverlay::clicked, this, &MainWindow::swapViews);

    m_stage = new OverlayStage([this](int w, int h) { layoutOverlays(w, h); });
    for (QWidget *w : {static_cast<QWidget *>(m_map), static_cast<QWidget *>(m_camera),
                       static_cast<QWidget *>(m_hud), static_cast<QWidget *>(m_messages),
                       static_cast<QWidget *>(m_warnings), static_cast<QWidget *>(m_status),
                       static_cast<QWidget *>(m_rail), static_cast<QWidget *>(m_dock),
                       static_cast<QWidget *>(m_pipFrame), static_cast<QWidget *>(m_connBar)})
        m_stage->add(w);
    root->addWidget(m_stage, 1);
    applyViewRoles();

    new QShortcut(QKeySequence(QStringLiteral("V")), this, [this] { swapViews(); });
    new QShortcut(QKeySequence(QStringLiteral("F11")), this, [this] { toggleFullscreen(); });
    new QShortcut(QKeySequence(Qt::Key_Escape), this, [this] { exitFullscreen(); });

    m_timer = new QTimer(this);
    connect(m_timer, &QTimer::timeout, this, &MainWindow::tick);
    m_timer->start(1000 / kRefreshHz);
}

QWidget *MainWindow::view(const QString &name)
{
    return name == QLatin1String("map") ? static_cast<QWidget *>(m_map)
                                        : static_cast<QWidget *>(m_camera);
}

QString MainWindow::pipName() const
{
    return m_primary == QLatin1String("map") ? QStringLiteral("cam") : QStringLiteral("map");
}

void MainWindow::applyViewRoles()
{
    QWidget *primary = view(m_primary);
    QWidget *pip = view(pipName());

    m_dock->setActive(m_primary);
    m_pipFrame->setCaption(pip == m_map ? QStringLiteral("MAP") : QStringLiteral("CAM"));

    primary->lower();
    for (QWidget *ov : {static_cast<QWidget *>(m_hud), static_cast<QWidget *>(m_messages),
                        static_cast<QWidget *>(m_warnings), static_cast<QWidget *>(m_rail),
                        static_cast<QWidget *>(m_dock)})
        ov->raise();
    pip->raise();
    m_pipFrame->raise();
    m_status->raise();
    m_connBar->raise();
}

void MainWindow::swapViews()
{
    m_primary = pipName();
    if (m_primary == QLatin1String("cam"))
        m_camera->ensureStarted();
    applyViewRoles();
    relayout();
}

void MainWindow::relayout()
{
    layoutOverlays(m_stage->width(), m_stage->height());
}

void MainWindow::layoutOverlays(int w, int h)
{
    const int m = 8;
    const bool tiny = (w < 560 || h < 380);
    const bool small = (w < 860 || h < 520);

    QWidget *primary = view(m_primary);
    QWidget *pip = view(pipName());
    primary->setGeometry(0, 0, w, h);
    primary->setVisible(true);

    m_status->setCompact(small);
    const int sbH = m_status->sizeHint().height();
    m_status->setGeometry(m, m, w - 2 * m, sbH);
    const int contentTop = m + sbH + m;

    const int railW = m_rail->sizeHint().width();
    const int railH = std::min(m_rail->sizeHint().height(), h - contentTop - m);
    m_rail->setGeometry(m, contentTop, railW, railH);
    const int railRight = m + railW;
    const int railBottom = contentTop + railH;

    const double prop = tiny ? 0.20 : (small ? 0.22 : 0.24);
    int pipW = int(clampd(w * prop, 92, 300));
    int pipH = int(pipW * 9 / 16);
    const int maxPipH = int(clampd(h * 0.30, 52, 320));
    if (pipH > maxPipH) {
        pipH = maxPipH;
        pipW = int(pipH * 16 / 9);
    }
    const int pipX = w - pipW - m;
    const int pipY = contentTop;
    pip->setVisible(true);
    m_pipFrame->setVisible(true);
    pip->setGeometry(pipX, pipY, pipW, pipH);
    m_pipFrame->setGeometry(pipX, pipY, pipW, pipH);

    m_map->setControlsCompact(small);
    m_camera->setHeaderCompact(small);
    const int dockW = std::min(m_dock->sizeHint().width(), w - railRight - 2 * m);
    const int dockH = m_dock->sizeHint().height();
    const int dockX = w - dockW - m;
    const int dockY = h - dockH - m;
    m_dock->setGeometry(dockX, dockY, dockW, dockH);

    int hudSz = int(clampd(std::min(w * 0.22, h * 0.34), tiny ? 84 : 110, 220));
    int hudH = hudSz + 28;
    const int availHud = h - m - (railBottom + m);
    if (hudH > availHud) {
        hudH = std::max(96, availHud);
        hudSz = std::max(72, std::min(hudSz, hudH - 28));
    }
    m_hud->setGeometry(m, h - hudH - m, hudSz, hudH);

    const int msgLeft = m + hudSz + m;
    const int msgRight = dockX - m;
    const int msgW = msgRight - msgLeft;
    const int msgH = int(clampd(h * 0.22, 90, 150));
    if (msgW >= 240 && !small) {
        m_messages->setGeometry(msgLeft, h - msgH - m, msgW, msgH);
        m_messages->setVisible(true);
    } else {
        m_messages->setVisible(false);
    }

    const int warnL = railRight + m;
    const int warnR = pipX - m;
    const int avail = std::max(0, warnR - warnL);
    const int warnW = avail > 200 ? int(clampd(std::min(avail, 600), 200, 600)) : std::max(avail, 120);
    const int warnX = warnL + std::max(0, (avail - warnW) / 2);
    m_warnings->setGeometry(warnX, contentTop, warnW, int(clampd(h * 0.4, 120, 200)));

    if (!m_connected) {
        const int cbW = std::min(m_connBar->sizeHint().width(), w - 2 * m);
        const int cbH = m_connBar->sizeHint().height();
        const int cbY = std::max(contentTop, (h - cbH) / 2);
        m_connBar->setGeometry((w - cbW) / 2, cbY, cbW, cbH);
    }
}

void MainWindow::onArm(bool force)
{
    if (confirm(QStringLiteral("Arm phương tiện?"),
                QStringLiteral("Động cơ có thể quay. Hãy chắc khu vực đã thông thoáng.")))
        m_controller->commands().arm(force);
}

void MainWindow::onTakeoff()
{
    bool ok = false;
    const double alt = QInputDialog::getDouble(
        this, QStringLiteral("Cất cánh"), QStringLiteral("Độ cao mục tiêu (m so với home):"),
        m_lastTakeoffAlt, 1.0, 1000.0, 1, &ok);
    if (!ok)
        return;
    if (!confirm(QStringLiteral("Cất cánh?"),
                 QStringLiteral("Phương tiện sẽ kiểm tra arm, chuyển GUIDED và leo lên %1 m. "
                                "Hãy chắc khu vực đã thông thoáng.").arg(alt, 0, 'f', 0)))
        return;
    m_lastTakeoffAlt = alt;
    m_controller->commands().takeoff(alt);
}

void MainWindow::onStartMission()
{
    if (confirm(QStringLiteral("Bắt đầu nhiệm vụ?"),
                QStringLiteral("Phương tiện sẽ chuyển sang AUTO và bay nhiệm vụ đã tải. "
                               "Hãy chắc nó đã arm và khu vực thông thoáng.")))
        m_controller->commands().startMission();
}

bool MainWindow::confirm(const QString &title, const QString &text)
{
    QMessageBox box(this);
    box.setIcon(QMessageBox::Warning);
    box.setWindowTitle(title);
    box.setText(text);
    box.setStandardButtons(QMessageBox::Yes | QMessageBox::No);
    box.setDefaultButton(QMessageBox::No);
    return box.exec() == QMessageBox::Yes;
}

void MainWindow::onConnect(const AppConfig &cfg)
{
    m_config = cfg;
    m_config.save();
    m_controller->connect(m_config.connectionString(), m_config.baud, m_config.label());
    setConnected(true);
}

void MainWindow::onDisconnect()
{
    m_controller->disconnect();
    setConnected(false);
}

void MainWindow::setConnected(bool connected)
{
    m_connected = connected;
    m_connBar->setConnected(connected);
    m_connBar->setVisible(!connected);
    m_status->setConnected(connected);
    if (connected)
        m_status->setLabel(m_config.label());
    m_rail->setConnected(connected);
    m_map->setConnected(connected);
    relayout();
}

void MainWindow::toggleFullscreen()
{
    if (isFullScreen())
        showNormal();
    else
        showFullScreen();
}

void MainWindow::exitFullscreen()
{
    if (isFullScreen())
        showNormal();
}

void MainWindow::tick()
{
    const auto snap = m_controller->snapshot();
    m_hud->updateFrom(snap);
    m_map->updateFrom(snap);
    if (m_connected) {
        m_status->updateFrom(snap);
        m_rail->updateFrom(snap);
    }

    for (const auto &notice : m_controller->drainNotices()) {
        m_messages->add(notice);
        if (MessagesPanel::isAlert(notice))
            m_warnings->push(notice);
    }
    m_warnings->prune();
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    m_timer->stop();
    m_camera->shutdown();
    m_controller->disconnect();
    m_config.save();
    QWidget::closeEvent(event);
}

} // namespace gcs::ui
