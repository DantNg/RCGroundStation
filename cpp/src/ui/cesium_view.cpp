#include "ui/cesium_view.h"

#ifdef HAVE_WEBENGINE

#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTcpServer>
#include <QTcpSocket>
#include <QUrl>
#include <QWebChannel>
#include <QWebEnginePage>

namespace gcs::ui {

// ── máy chủ HTTP localhost tĩnh cho Cesium + trang ──────────────────────────
// Cesium tải dữ liệu toán học và bộ giải mã từ Web Worker mà trình duyệt từ chối
// khởi động từ origin ``file://``, nên cần một origin http thật. Phục vụ cục bộ
// nghĩa là engine 3D chạy hoàn toàn ngoại tuyến.
namespace {

QString mimeFor(const QString &path)
{
    const QString p = path.toLower();
    if (p.endsWith(".html")) return "text/html";
    if (p.endsWith(".js"))   return "application/javascript";
    if (p.endsWith(".css"))  return "text/css";
    if (p.endsWith(".json")) return "application/json";
    if (p.endsWith(".wasm")) return "application/wasm";
    if (p.endsWith(".png"))  return "image/png";
    if (p.endsWith(".jpg") || p.endsWith(".jpeg")) return "image/jpeg";
    if (p.endsWith(".gif"))  return "image/gif";
    if (p.endsWith(".svg"))  return "image/svg+xml";
    if (p.endsWith(".ktx2")) return "image/ktx2";
    if (p.endsWith(".glb"))  return "model/gltf-binary";
    return "application/octet-stream";
}

class StaticHttpServer : public QTcpServer {
public:
    explicit StaticHttpServer(QString root) : m_root(std::move(root)) {}

protected:
    void incomingConnection(qintptr handle) override
    {
        auto *sock = new QTcpSocket(this);
        sock->setSocketDescriptor(handle);
        connect(sock, &QTcpSocket::readyRead, this, [this, sock] { serve(sock); });
        connect(sock, &QTcpSocket::disconnected, sock, &QObject::deleteLater);
    }

private:
    void serve(QTcpSocket *sock)
    {
        const QByteArray req = sock->readAll();
        const int sp1 = req.indexOf(' ');
        const int sp2 = req.indexOf(' ', sp1 + 1);
        if (sp1 < 0 || sp2 < 0) {
            sock->disconnectFromHost();
            return;
        }
        QString path = QString::fromUtf8(req.mid(sp1 + 1, sp2 - sp1 - 1));
        const int q = path.indexOf('?');
        if (q >= 0)
            path = path.left(q);
        path = QUrl::fromPercentEncoding(path.toUtf8());
        if (path == "/" || path.isEmpty())
            path = "/cesium_map.html";

        const QString full = QDir::cleanPath(m_root + path);
        if (!full.startsWith(m_root)) {
            writeStatus(sock, 403, "Forbidden");
            return;
        }
        QFile f(full);
        if (!f.exists() || !f.open(QIODevice::ReadOnly)) {
            writeStatus(sock, 404, "Not Found");
            return;
        }
        const QByteArray body = f.readAll();
        QByteArray header = "HTTP/1.1 200 OK\r\n";
        header += "Content-Type: " + mimeFor(full).toUtf8() + "\r\n";
        header += "Content-Length: " + QByteArray::number(body.size()) + "\r\n";
        header += "Access-Control-Allow-Origin: *\r\n";
        header += "Connection: close\r\n\r\n";
        sock->write(header);
        sock->write(body);
        sock->flush();
        sock->disconnectFromHost();
    }

    void writeStatus(QTcpSocket *sock, int code, const char *text)
    {
        QByteArray resp = "HTTP/1.1 " + QByteArray::number(code) + " " + text + "\r\n";
        resp += "Content-Length: 0\r\nConnection: close\r\n\r\n";
        sock->write(resp);
        sock->flush();
        sock->disconnectFromHost();
    }

    QString m_root;
};

QString ensureServer()
{
    static QString base;
    if (!base.isEmpty())
        return base;
    const QString assets = QDir(QCoreApplication::applicationDirPath()).filePath("assets");
    auto *server = new StaticHttpServer(QDir::cleanPath(assets));
    if (server->listen(QHostAddress::LocalHost, 0))
        base = QStringLiteral("http://127.0.0.1:%1").arg(server->serverPort());
    return base;
}

QString b(bool v) { return v ? QStringLiteral("true") : QStringLiteral("false"); }

} // namespace

// ── cầu nối JS → C++ (đăng ký trên trang là ``bridge``) ─────────────────────
class CesiumBridge : public QObject {
    Q_OBJECT
public:
    explicit CesiumBridge(QObject *parent = nullptr) : QObject(parent) {}

signals:
    void waypointAdded(double lat, double lon);
    void waypointMoved(int idx, double lat, double lon);
    void waypointRemoved(int idx);
    void waypointAltChanged(int idx, double deltaM);
    void waypointClicked(int idx);

public slots:
    void add_waypoint(double lat, double lon) { emit waypointAdded(lat, lon); }
    void select_waypoint(int idx) { emit waypointClicked(idx); }
    void move_waypoint(int idx, double lat, double lon) { emit waypointMoved(idx, lat, lon); }
    void remove_waypoint(int idx) { emit waypointRemoved(idx); }
    void adjust_waypoint_alt(int idx, double delta) { emit waypointAltChanged(idx, delta); }
};

CesiumView::CesiumView(QWidget *parent) : CesiumViewBase(parent)
{
    m_bridge = new CesiumBridge(this);
    connect(m_bridge, &CesiumBridge::waypointAdded, this, &CesiumView::waypointAdded);
    connect(m_bridge, &CesiumBridge::waypointMoved, this, &CesiumView::waypointMoved);
    connect(m_bridge, &CesiumBridge::waypointRemoved, this, &CesiumView::waypointRemoved);
    connect(m_bridge, &CesiumBridge::waypointAltChanged, this, &CesiumView::waypointAltChanged);
    connect(m_bridge, &CesiumBridge::waypointClicked, this, &CesiumView::waypointClicked);

    auto *channel = new QWebChannel(this);
    channel->registerObject(QStringLiteral("bridge"), m_bridge);
    page()->setWebChannel(channel);

    connect(this, &QWebEngineView::loadFinished, this, &CesiumView::onLoaded);
    load(QUrl(ensureServer() + QStringLiteral("/cesium_map.html")));
}

void CesiumView::onLoaded(bool ok)
{
    m_ready = ok;
    if (!ok)
        return;
    for (const QString &code : m_pending)
        page()->runJavaScript(code);
    m_pending.clear();
}

void CesiumView::js(const QString &code)
{
    if (m_ready)
        page()->runJavaScript(code);
    else
        m_pending << code;
}

void CesiumView::setVehicle(double lat, double lon, double alt, double heading, bool haveFix)
{
    js(QStringLiteral("gcsSetVehicle(%1,%2,%3,%4,%5)")
           .arg(lat, 0, 'g', 12).arg(lon, 0, 'g', 12).arg(alt).arg(heading).arg(b(haveFix)));
}

void CesiumView::setWaypoints(const std::vector<domain::Waypoint> &waypoints)
{
    QJsonArray arr;
    for (const auto &wp : waypoints) {
        QJsonObject o;
        o["lat"] = wp.lat;
        o["lon"] = wp.lon;
        o["alt"] = wp.alt;
        arr.append(o);
    }
    const QString json = QString::fromUtf8(QJsonDocument(arr).toJson(QJsonDocument::Compact));
    js(QStringLiteral("gcsSetWaypoints(%1)").arg(json));
}

void CesiumView::setSim(double lat, double lon, double alt, double heading)
{
    js(QStringLiteral("gcsSetSim(%1,%2,%3,%4)")
           .arg(lat, 0, 'g', 12).arg(lon, 0, 'g', 12).arg(alt).arg(heading));
}

void CesiumView::clearSim() { js(QStringLiteral("gcsClearSim()")); }
void CesiumView::setEditMode(bool on) { js(QStringLiteral("gcsSetEdit(%1)").arg(b(on))); }
void CesiumView::setFollow(bool on) { js(QStringLiteral("gcsSetFollow(%1)").arg(b(on))); }
void CesiumView::flyToVehicle() { js(QStringLiteral("gcsFlyToVehicle()")); }

} // namespace gcs::ui

#include "cesium_view.moc"

#else // ── Không có WebEngine: placeholder giữ nguyên API ──────────────────────

#include <QLabel>
#include <QVBoxLayout>

namespace gcs::ui {

CesiumView::CesiumView(QWidget *parent) : CesiumViewBase(parent)
{
    auto *lay = new QVBoxLayout(this);
    auto *lbl = new QLabel(
        QStringLiteral("Chế độ 3D không khả dụng trong bản dựng này\n(thiếu Qt WebEngine)."));
    lbl->setAlignment(Qt::AlignCenter);
    lbl->setWordWrap(true);
    lbl->setStyleSheet(QStringLiteral("color:#aab6c4; background:#0a0d12; font-size:14px;"));
    lay->setContentsMargins(0, 0, 0, 0);
    lay->addWidget(lbl);
}

void CesiumView::setVehicle(double, double, double, double, bool) {}
void CesiumView::setWaypoints(const std::vector<domain::Waypoint> &) {}
void CesiumView::setSim(double, double, double, double) {}
void CesiumView::clearSim() {}
void CesiumView::setEditMode(bool) {}
void CesiumView::setFollow(bool) {}
void CesiumView::flyToVehicle() {}

} // namespace gcs::ui

#endif
