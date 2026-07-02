// Trạm Điều Khiển Mặt Đất — điểm vào desktop (Windows / Linux), bản C++/Qt.
//
// Cửa sổ mở ra ở trạng thái chưa kết nối; chọn một đường truyền (Serial / UDP /
// TCP) trên thanh trên và nhấn Kết nối. Để thử không cần phần cứng, chạy bộ mô
// phỏng ở cửa sổ thứ hai và kết nối qua UDP cổng 14550.
#include "app/controller.h"
#include "config.h"
#include "ui/main_window.h"

#include <QApplication>

#include <cstdlib>

int main(int argc, char *argv[])
{
    // QtWebEngine (bản đồ Cesium 3D) cần cho phép Chromium quay lui về WebGL
    // phần mềm (SwiftShader) trên máy có GPU bị chặn hoặc không có WebGL, nếu
    // không quả cầu vẽ toàn màu đen. Phải đặt trước khi QApplication khởi tạo.
#ifdef _WIN32
    _putenv_s("QTWEBENGINE_CHROMIUM_FLAGS",
              "--ignore-gpu-blocklist --enable-unsafe-swiftshader --enable-webgl "
              "--use-gl=angle --use-angle=swiftshader");
#else
    setenv("QTWEBENGINE_CHROMIUM_FLAGS",
           "--ignore-gpu-blocklist --enable-unsafe-swiftshader --enable-webgl", 0);
#endif

    // QtWebEngine chia sẻ ngữ cảnh GL với phần còn lại của ứng dụng; phải đặt
    // trước khi dựng QApplication.
    QApplication::setAttribute(Qt::AA_ShareOpenGLContexts);
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Trạm Điều Khiển Mặt Đất"));
    app.setApplicationDisplayName(QStringLiteral("Trạm Điều Khiển Mặt Đất — Desktop"));

    gcs::AppConfig config = gcs::AppConfig::load();
    gcs::app::GcsController controller;
    gcs::ui::MainWindow window(&controller, config);
    window.setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    window.show();
    return app.exec();
}
