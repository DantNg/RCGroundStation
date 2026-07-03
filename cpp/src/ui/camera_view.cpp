#include "ui/camera_view.h"

#include "domain/telemetry.h"
#include "interfaces/vision.h"
#include "ui/video_display.h"
#include "vision/centering_policy.h"
#include "vision/centroid_tracker.h"
#include "vision/motion_detector.h"
#include "vision/vision_pipeline.h"

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
#include <QVideoFrame>
#include <QVideoSink>

namespace gcs::ui {

CameraView::CameraView(QWidget *parent) : QWidget(parent)
{
    // Pipeline thị giác: detector (placeholder AI) → tracker → policy gimbal.
    // Giữ con trỏ policy để bật/tắt gửi lệnh; quyền sở hữu chuyển vào pipeline.
    auto detector = std::make_unique<vision::MotionDetector>();
    auto tracker = std::make_unique<vision::CentroidTracker>();
    auto policy = std::make_unique<vision::CenteringPolicy>();
    m_policy = policy.get();
    m_pipeline = std::make_unique<vision::VisionPipeline>(
        std::move(detector), std::move(tracker), std::move(policy));

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
    m_sink = new QVideoSink(this);
    m_session->setVideoSink(m_sink);
    connect(m_sink, &QVideoSink::videoFrameChanged, this, &CameraView::onFrame);

    m_display = new VideoDisplay;
    m_stack->addWidget(m_display);

    m_mediaDevices = new QMediaDevices(this);
    connect(m_mediaDevices, &QMediaDevices::videoInputsChanged, this, &CameraView::refreshDevices);

    outer->addWidget(bodyW, 1);

    refreshDevices();
    showVideo(false);
}

CameraView::~CameraView() = default;

void CameraView::setCommandSinkProvider(std::function<interfaces::ICommandSink *()> provider)
{
    m_sinkProvider = std::move(provider);
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

    m_trackBtn = new QPushButton(QStringLiteral("Bắt bám"));
    m_trackBtn->setObjectName("Ghost");
    m_trackBtn->setCursor(Qt::PointingHandCursor);
    m_trackBtn->setToolTip(QStringLiteral("Bật bám mục tiêu — lái gimbal theo người"));
    connect(m_trackBtn, &QPushButton::clicked, this, &CameraView::toggleTracking);
    row->addWidget(m_trackBtn);

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
    m_pipeline->reset();
    m_display->clear();
    setRunning(false);
    m_placeholder->setText(QStringLiteral("Camera tắt"));
}

void CameraView::onFrame()
{
    if (!m_running)
        return;
    const QVideoFrame vf = m_sink->videoFrame();
    QImage img = vf.toImage();
    if (img.isNull())
        return;
    if (img.format() != QImage::Format_RGB32 && img.format() != QImage::Format_ARGB32)
        img = img.convertToFormat(QImage::Format_RGB32);

    interfaces::VideoFrame frame;
    frame.image = img;
    frame.timestampMs = domain::nowMs();

    interfaces::ICommandSink *sink = m_sinkProvider ? m_sinkProvider() : nullptr;
    const interfaces::TrackResult res = m_pipeline->process(frame, sink);

    m_display->setFrame(img);
    m_display->setResult(res);
}

void CameraView::toggleTracking()
{
    const bool on = !m_pipeline->trackingEnabled();
    m_pipeline->setTrackingEnabled(on);
    m_policy->setEnabled(on);
    m_trackBtn->setText(on ? QStringLiteral("Đang bám") : QStringLiteral("Bắt bám"));
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
    if (m_display)
        m_display->setVisible(show);
    m_placeholder->setVisible(!show);
}

void CameraView::shutdown()
{
    stop();
}

} // namespace gcs::ui
