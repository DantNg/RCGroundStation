// View webcam trực tiếp — view thứ hai có thể toàn màn bên cạnh bản đồ.
//
// Dùng Qt Multimedia (QCamera → QMediaCaptureSession → QVideoWidget) để chạy
// trên Windows/Linux với camera do hệ điều hành cung cấp. View tự chứa: một dải
// tiêu đề chọn thiết bị và bật/tắt, một placeholder hiện đến khi có khung hình.
#pragma once

#include <QWidget>

class QComboBox;
class QLabel;
class QPushButton;
class QStackedLayout;
class QCamera;
class QMediaCaptureSession;
class QMediaDevices;
class QVideoWidget;

namespace gcs::ui {

class CameraView : public QWidget {
    Q_OBJECT
public:
    explicit CameraView(QWidget *parent = nullptr);

    QWidget *headerWidget() { return m_header; }
    void setHeaderCompact(bool compact);
    bool ensureStarted();
    bool isRunning() const { return m_running; }
    void shutdown();

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

    bool m_running = false;
    QWidget *m_header = nullptr;
    QLabel *m_camLabel = nullptr;
    QComboBox *m_device = nullptr;
    QPushButton *m_startBtn = nullptr;
    QLabel *m_placeholder = nullptr;
    QStackedLayout *m_stack = nullptr;
    QCamera *m_camera = nullptr;
    QMediaCaptureSession *m_session = nullptr;
    QMediaDevices *m_mediaDevices = nullptr;
    QVideoWidget *m_video = nullptr;
};

} // namespace gcs::ui
