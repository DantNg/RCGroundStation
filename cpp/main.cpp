// Trạm Điều Khiển Mặt Đất — điểm vào desktop (Windows / Linux), giao diện QML.
//
// Dựng lại lớp UI bằng QML/Qt Quick theo mockup "GroundController Handheld"
// (bản đồ 2D vệ tinh + PFD tổng hợp + camera FPV, chuyển đổi trong khung 800×480).
// Backend C++ giữ nguyên: GcsController sở hữu link/telemetry/lệnh; hai lớp cầu
// nối TelemetryViewModel (đọc) và Backend (lệnh) lộ ra cho QML qua context prop.
//
// Cửa sổ mở ở trạng thái chưa kết nối; chọn đường truyền (UDP/TCP/Serial) rồi
// Kết nối. Thử không cần phần cứng: chạy bộ mô phỏng rồi kết nối UDP cổng 14550.
#include "app/controller.h"
#include "bridge/backend.h"
#include "bridge/telemetry_view_model.h"
#include "config.h"
#include "domain/roles.h"

#include <QCoreApplication>
#include <QGuiApplication>
#include <QQmlContext>
#include <QQmlApplicationEngine>
#include <QQuickStyle>
#include <QQuickWindow>
#include <QSslSocket>
#include <QTimer>
#include <QUrl>

#include <cstdlib>

int main(int argc, char *argv[])
{
    // TLS cho tải tile bản đồ vệ tinh (https). Trên Windows, Qt mặc định dùng
    // OpenSSL; nếu bản dựng không kèm OpenSSL 3 thì mọi request https thất bại và
    // map không tải được tile. Chuyển sang Schannel (TLS sẵn của Windows, không
    // cần DLL ngoài). Phải đặt trước request mạng đầu tiên. Linux bỏ qua an toàn.
    if (QSslSocket::availableBackends().contains(QStringLiteral("schannel"))
        && QSslSocket::activeBackend() != QStringLiteral("schannel")) {
        QSslSocket::setActiveBackend(QStringLiteral("schannel"));
    }

    QGuiApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("Trạm Điều Khiển Mặt Đất"));
    app.setApplicationDisplayName(QStringLiteral("Trạm Điều Khiển Mặt Đất — Desktop"));

    // Kiểu "Basic" cho Quick Controls — trung tính, để ta tự tạo dáng theo mockup
    // (các style Material/Fusion sẽ đè màu/nền của thiết kế). Phải đặt trước engine.
    QQuickStyle::setStyle(QStringLiteral("Basic"));

    gcs::AppConfig config = gcs::AppConfig::load();
    gcs::app::GcsController controller(gcs::domain::roleFromString(config.role));

    gcs::bridge::TelemetryViewModel telemetry(&controller);
    gcs::bridge::Backend backend(&controller, config);

    QQmlApplicationEngine engine;
    engine.rootContext()->setContextProperty(QStringLiteral("telemetry"), &telemetry);
    engine.rootContext()->setContextProperty(QStringLiteral("backend"), &backend);

    // Kho định nghĩa nhà cung cấp tile cho plugin OSM của Qt Location. Bản đồ 2D
    // vệ tinh (Esri World Imagery) dùng thứ tự tile z/y/x — plugin custom.host chỉ
    // hỗ trợ z/x/y.png, nên ta trỏ providersrepository.address tới thư mục cục bộ
    // "providers/" (cạnh binary) chứa file "satellite" (Esri) và "street" (OSM).
    const QString providersUrl =
        QUrl::fromLocalFile(QCoreApplication::applicationDirPath() + QStringLiteral("/providers/"))
            .toString();
    engine.rootContext()->setContextProperty(QStringLiteral("mapProvidersUrl"), providersUrl);

    // Chẩn đoán bản đồ (in ra stderr) — hữu ích khi map trắng trên Linux/Pi.
    // Tile Esri/OSM tải qua https nên cần backend TLS: nếu supportsSsl()=false thì
    // mọi tile fail và map trắng — khi đó cài openssl + ca-certificates.
    qInfo() << "[map] providers:" << providersUrl;
    qInfo() << "[map] TLS backends:" << QSslSocket::availableBackends()
            << "| active:" << QSslSocket::activeBackend()
            << "| supportsSsl:" << QSslSocket::supportsSsl();

    QObject::connect(
        &engine, &QQmlApplicationEngine::objectCreationFailed, &app,
        []() { std::exit(EXIT_FAILURE); }, Qt::QueuedConnection);
    engine.loadFromModule("GroundCtrl", "Main");

    // Tiện ích thử nghiệm: tự kết nối UDP nếu đặt biến môi trường LITEGCS_UDP=<cổng>
    // (vd LITEGCS_UDP=14550) — bỏ qua bước bấm "KẾT NỐI" khi test với bộ mô phỏng.
    if (qEnvironmentVariableIsSet("LITEGCS_UDP"))
        backend.connectUdp(qEnvironmentVariable("LITEGCS_UDP").toInt());

    if (qEnvironmentVariableIsSet("LITEGCS_GRAB") && !engine.rootObjects().isEmpty()) {
        if (auto *w = qobject_cast<QQuickWindow *>(engine.rootObjects().first())) {
            const QString pfx = qEnvironmentVariable("LITEGCS_GRAB");
            QTimer::singleShot(2500, [w, pfx] { w->grabWindow().save(pfx + ".png"); });
            QTimer::singleShot(3000, &app, &QGuiApplication::quit);
        }
    }

    return app.exec();
}
