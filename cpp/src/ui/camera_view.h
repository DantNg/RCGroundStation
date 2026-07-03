// View webcam/VRX trực tiếp — view thứ hai có thể toàn màn bên cạnh bản đồ.
//
// Dùng Qt Multimedia (QCamera → QMediaCaptureSession → QVideoSink) để lấy khung
// trên Windows/Linux từ thiết bị do hệ điều hành cung cấp. Module VRX cắm USB
// xuất hiện như một thiết bị UVC nên được liệt kê chung ở đây. Mỗi khung được
// đẩy qua VisionPipeline (phát hiện người → bắt bám) rồi vẽ kèm overlay bởi
// VideoDisplay. View tự chứa: một dải tiêu đề chọn thiết bị/bật-tắt/bắt-bám.
#pragma once

#include <QWidget>

#include <functional>
#include <memory>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedLayout;
class QCamera;
class QMediaCaptureSession;
class QMediaDevices;
class QVideoSink;

namespace gcs::interfaces { class ICommandSink; }
namespace gcs::vision { class VisionPipeline; class CenteringPolicy; }

namespace gcs::ui {

class VideoDisplay;

class CameraView : public QWidget {
    Q_OBJECT
public:
    explicit CameraView(QWidget *parent = nullptr);
    ~CameraView() override;

    QWidget *headerWidget() { return m_header; }
    void setHeaderCompact(bool compact);
    bool ensureStarted();
    bool isRunning() const { return m_running; }
    void shutdown();

    // Nguồn command sink sống (lấy mới mỗi khung để không giữ tham chiếu cũ qua
    // các lần kết nối lại). Chính sách bắt bám gửi lệnh gimbal qua đây.
    void setCommandSinkProvider(std::function<interfaces::ICommandSink *()> provider);

signals:
    void runningChanged(bool running);

protected:
    void resizeEvent(QResizeEvent *e) override;

private:
    QWidget *buildHeader();
    void refreshDevices();
    void onDeviceChanged(int idx);
    void toggle();
    void stop();
    void setRunning(bool running);
    void showVideo(bool show);
    void onFrame();
    void toggleTracking();

    bool m_running = false;
    QWidget *m_header = nullptr;
    QLabel *m_camLabel = nullptr;
    QComboBox *m_device = nullptr;
    QPushButton *m_startBtn = nullptr;
    QPushButton *m_trackBtn = nullptr;
    QLabel *m_placeholder = nullptr;
    QStackedLayout *m_stack = nullptr;
    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_session = nullptr;
    QMediaDevices *m_mediaDevices = nullptr;
    QVideoSink *m_sink = nullptr;
    VideoDisplay *m_display = nullptr;

    std::unique_ptr<vision::VisionPipeline> m_pipeline;
    vision::CenteringPolicy *m_policy = nullptr;   // không sở hữu (pipeline giữ)
    std::function<interfaces::ICommandSink *()> m_sinkProvider;
};

} // namespace gcs::ui
