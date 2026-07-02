#include "ui/camera_view.h"

#include <QCamera>
#include <QCameraDevice>
#include <QComboBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QMediaCaptureSession>
#include <QMediaDevices>
#include <QPushButton>
#include <QStackedLayout>
#include <QVBoxLayout>
#include <QVideoWidget>

namespace gcs::ui {

CameraView::CameraView(QWidget *parent) : QWidget(parent)
{
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    m_header = buildHeader();

    auto *bodyW = new QWidget;
    m_stack = new QStackedLayout(bodyW);
    m_stack->setStackingMode(QStackedLayout::StackAll);
    m_stack->setContentsMargins(0, 0, 0, 0);

    m_placeholder = new QLabel(QStringLiteral("Camera tắt"));
    m_placeholder->setObjectName("CamPlaceholder");
    m_placeholder->setAlignment(Qt::AlignCenter);
    m_placeholder->setWordWrap(true);
    m_placeholder->setStyleSheet(QStringLiteral("background-color: #05080c;"));
    m_stack->addWidget(m_placeholder);

    m_session = new QMediaCaptureSession(this);
    m_video = new QVideoWidget;
    m_video->setStyleSheet(QStringLiteral("background-color: #05080c;"));
    m_session->setVideoOutput(m_video);
    m_stack->addWidget(m_video);
    m_mediaDevices = new QMediaDevices(this);
    connect(m_mediaDevices, &QMediaDevices::videoInputsChanged, this, &CameraView::refreshDevices);

    outer->addWidget(bodyW, 1);

    refreshDevices();
    showVideo(false);
}

QWidget *CameraView::buildHeader()
{
    auto *bar = new QWidget;
    bar->setAttribute(Qt::WA_TranslucentBackground, true);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);

    m_camLabel = new QLabel(QStringLiteral("CAM"));
    m_camLabel->setObjectName("PanelTitle");
    row->addWidget(m_camLabel);

    m_device = new QComboBox;
    m_device->setMinimumWidth(96);
    connect(m_device, &QComboBox::currentIndexChanged, this, &CameraView::onDeviceChanged);
    row->addWidget(m_device, 1);

    auto *refresh = new QPushButton(QStringLiteral("⟳"));
    refresh->setObjectName("IconButton");
    refresh->setToolTip(QStringLiteral("Làm mới danh sách camera"));
    refresh->setCursor(Qt::PointingHandCursor);
    connect(refresh, &QPushButton::clicked, this, &CameraView::refreshDevices);
    row->addWidget(refresh);

    m_startBtn = new QPushButton(QStringLiteral("Bật"));
    m_startBtn->setObjectName("Ghost");
    m_startBtn->setCursor(Qt::PointingHandCursor);
    connect(m_startBtn, &QPushButton::clicked, this, &CameraView::toggle);
    row->addWidget(m_startBtn);
    return bar;
}

void CameraView::setHeaderCompact(bool compact)
{
    m_camLabel->setVisible(!compact);
}

void CameraView::resizeEvent(QResizeEvent *e)
{
    QWidget::resizeEvent(e);
    const int side = std::min(width(), height());
    const int px = std::max(8, std::min(20, int(side * 0.12)));
    m_placeholder->setStyleSheet(
        QStringLiteral("background-color: #05080c; font-size: %1px;").arg(px));
}

void CameraView::refreshDevices()
{
    QByteArray prevId;
    if (m_device->currentIndex() >= 0)
        prevId = m_device->currentData().value<QCameraDevice>().id();

    m_device->blockSignals(true);
    m_device->clear();
    const QList<QCameraDevice> cams = QMediaDevices::videoInputs();
    for (const QCameraDevice &cam : cams)
        m_device->addItem(cam.description(), QVariant::fromValue(cam));
    m_device->blockSignals(false);

    if (cams.isEmpty()) {
        m_placeholder->setText(QStringLiteral("Không có camera"));
        m_startBtn->setEnabled(false);
        return;
    }
    m_startBtn->setEnabled(true);
    for (int i = 0; i < m_device->count(); ++i) {
        if (m_device->itemData(i).value<QCameraDevice>().id() == prevId && !prevId.isEmpty()) {
            m_device->setCurrentIndex(i);
            break;
        }
    }
    if (!m_running)
        m_placeholder->setText(QStringLiteral("Camera tắt"));
}

void CameraView::onDeviceChanged(int)
{
    if (m_running) {
        stop();
        ensureStarted();
    }
}

void CameraView::toggle()
{
    if (m_running)
        stop();
    else
        ensureStarted();
}

bool CameraView::ensureStarted()
{
    if (m_running)
        return true;
    if (m_device->currentIndex() < 0)
        return false;
    const QCameraDevice dev = m_device->currentData().value<QCameraDevice>();
    if (dev.isNull())
        return false;
    m_camera = new QCamera(dev, this);
    connect(m_camera, &QCamera::errorOccurred, this, [this](QCamera::Error, const QString &msg) {
        m_placeholder->setText(QStringLiteral("Lỗi camera:\n%1")
                                   .arg(msg.isEmpty() ? QStringLiteral("không khả dụng") : msg));
        setRunning(false);
    });
    m_session->setCamera(m_camera);
    m_camera->start();
    setRunning(true);
    return true;
}

void CameraView::stop()
{
    if (m_camera) {
        m_camera->stop();
        m_session->setCamera(nullptr);
        m_camera->deleteLater();
        m_camera = nullptr;
    }
    setRunning(false);
    m_placeholder->setText(QStringLiteral("Camera tắt"));
}

void CameraView::setRunning(bool running)
{
    if (running == m_running) {
        showVideo(running);
        return;
    }
    m_running = running;
    m_startBtn->setText(running ? QStringLiteral("Tắt") : QStringLiteral("Bật"));
    showVideo(running);
    emit runningChanged(running);
}

void CameraView::showVideo(bool show)
{
    if (m_video)
        m_video->setVisible(show);
    m_placeholder->setVisible(!show);
}

void CameraView::shutdown()
{
    stop();
}

} // namespace gcs::ui
