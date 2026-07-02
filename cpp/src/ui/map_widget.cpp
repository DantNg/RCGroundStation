#include "ui/map_widget.h"

#include "ui/cesium_view.h"
#include "ui/theme.h"
#include "ui/waypoint_editor.h"

#include <QComboBox>
#include <QContextMenuEvent>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QLabel>
#include <QMenu>
#include <QMouseEvent>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QPainter>
#include <QPolygonF>
#include <QPushButton>
#include <QStackedWidget>
#include <QTimer>
#include <QVBoxLayout>
#include <QWheelEvent>
#include <QWidgetAction>

#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

namespace gcs::ui {

namespace {
constexpr int TILE = 256;
constexpr int kSimHz = 30;
const char *kUserAgent = "LiteGCS-Desktop/1.0 (+https://github.com/)";

QString cacheDir()
{
    return QDir::homePath() + QStringLiteral("/.lite_gcs/tiles");
}

const QHash<QString, TileProvider> &providers()
{
    static const QHash<QString, TileProvider> p = {
        {"Satellite", TileProvider{"Satellite",
            "https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/"
            "MapServer/tile/{z}/{y}/{x}", 19}},
        {"Street", TileProvider{"Street",
            "https://tile.openstreetmap.org/{z}/{x}/{y}.png", 19}},
    };
    return p;
}

std::pair<double, double> lonlatToWorldPx(double lat, double lon, int z)
{
    const double n = std::pow(2.0, z);
    lat = std::max(std::min(lat, 85.05112878), -85.05112878);
    const double latRad = lat * M_PI / 180.0;
    const double x = (lon + 180.0) / 360.0 * n;
    const double y = (1.0 - std::asinh(std::tan(latRad)) / M_PI) / 2.0 * n;
    return {x * TILE, y * TILE};
}

std::pair<double, double> worldPxToLonlat(double px, double py, int z)
{
    const double n = std::pow(2.0, z);
    const double lon = px / TILE / n * 360.0 - 180.0;
    const double ty = py / TILE / n;
    const double lat = std::atan(std::sinh(M_PI * (1 - 2 * ty))) * 180.0 / M_PI;
    return {lat, lon};
}
}

QString TileProvider::url(int z, int x, int y) const
{
    QString u = urlTemplate;
    u.replace("{z}", QString::number(z));
    u.replace("{x}", QString::number(x));
    u.replace("{y}", QString::number(y));
    return u;
}

// ── TileLoader ───────────────────────────────────────────────────────────────
TileLoader::TileLoader(QObject *parent)
    : QObject(parent), m_net(new QNetworkAccessManager(this))
{
}

QString TileLoader::keyOf(const QString &name, int z, int x, int y)
{
    return QStringLiteral("%1/%2/%3/%4").arg(name).arg(z).arg(x).arg(y);
}

QString TileLoader::diskPath(const QString &name, int z, int x, int y)
{
    return QStringLiteral("%1/%2/%3/%4/%5.png").arg(cacheDir(), name).arg(z).arg(x).arg(y);
}

QImage TileLoader::get(const TileProvider &provider, int z, int x, int y)
{
    const QString key = keyOf(provider.name, z, x, y);
    auto it = m_mem.find(key);
    if (it != m_mem.end())
        return it.value();
    if (m_failed.contains(key))
        return QImage();
    const QString disk = diskPath(provider.name, z, x, y);
    if (QFileInfo::exists(disk)) {
        QImage img(disk);
        if (!img.isNull()) {
            m_mem.insert(key, img);
            return img;
        }
    }
    if (!m_pending.contains(key)) {
        m_pending.insert(key);
        QNetworkRequest req{QUrl(provider.url(z, x, y))};
        req.setHeader(QNetworkRequest::UserAgentHeader, QString::fromLatin1(kUserAgent));
        QNetworkReply *reply = m_net->get(req);
        const QString name = provider.name;
        connect(reply, &QNetworkReply::finished, this, [this, reply, key, name, z, x, y] {
            reply->deleteLater();
            m_pending.remove(key);
            if (reply->error() != QNetworkReply::NoError) {
                m_failed.insert(key);
                return;
            }
            const QByteArray data = reply->readAll();
            QImage img;
            if (!img.loadFromData(data)) {
                m_failed.insert(key);
                return;
            }
            const QString path = diskPath(name, z, x, y);
            QDir().mkpath(QFileInfo(path).absolutePath());
            QFile f(path);
            if (f.open(QIODevice::WriteOnly))
                f.write(data);
            m_mem.insert(key, img);
            emit ready();
        });
    }
    return QImage();
}

// ── MapCanvas ────────────────────────────────────────────────────────────────
// Bộ vẽ ô/dấu thực sự (tách khỏi hàng điều khiển).
class MapCanvas : public QWidget {
public:
    explicit MapCanvas(MapWidget *owner) : QWidget(owner), m_o(owner)
    {
        setMinimumSize(320, 240);
        setMouseTracking(true);
    }

protected:
    void paintEvent(QPaintEvent *) override;
    void contextMenuEvent(QContextMenuEvent *e) override;
    void mousePressEvent(QMouseEvent *e) override;
    void mouseMoveEvent(QMouseEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void wheelEvent(QWheelEvent *e) override;

private:
    void drawTrail(QPainter &p, double ox, double oy, int z);
    void drawMission(QPainter &p, double ox, double oy, int z);
    void drawTarget(QPainter &p, double ox, double oy, int z);
    void drawMarker(QPainter &p, double ox, double oy, int z);
    void drawSim(QPainter &p, double ox, double oy, int z);
    void drawHudText(QPainter &p);
    std::pair<double, double> screenToLonlat(const QPointF &pos);
    std::optional<int> wpAt(const QPointF &pos);

    MapWidget *m_o;
    std::optional<QPointF> m_press;
    bool m_moved = false;
    std::optional<QPointF> m_dragLast;
};

void MapCanvas::paintEvent(QPaintEvent *)
{
    MapWidget *o = m_o;
    QPainter p(this);
    p.fillRect(rect(), QColor("#0a0d12"));
    const int w = width(), h = height();
    const int z = o->m_zoom;
    const auto [cxPx, cyPx] = lonlatToWorldPx(o->m_centerLat, o->m_centerLon, z);
    const double originX = cxPx - w / 2.0;
    const double originY = cyPx - h / 2.0;

    const int n = int(std::pow(2.0, z));
    const int tx0 = int(std::floor(originX / TILE));
    const int ty0 = int(std::floor(originY / TILE));
    const int tx1 = int(std::floor((originX + w) / TILE));
    const int ty1 = int(std::floor((originY + h) / TILE));
    for (int ty = ty0; ty <= ty1; ++ty) {
        if (ty < 0 || ty >= n)
            continue;
        for (int tx = tx0; tx <= tx1; ++tx) {
            const int wx = ((tx % n) + n) % n;
            const double sx = tx * TILE - originX;
            const double sy = ty * TILE - originY;
            const QImage img = o->m_loader->get(o->m_provider, z, wx, ty);
            if (!img.isNull()) {
                p.drawImage(QPointF(sx, sy), img);
            } else {
                p.fillRect(int(sx), int(sy), TILE, TILE, QColor("#1b2230"));
                p.setPen(QPen(QColor("#222b3a")));
                p.drawRect(int(sx), int(sy), TILE, TILE);
            }
        }
    }

    drawTrail(p, originX, originY, z);
    drawMission(p, originX, originY, z);
    drawTarget(p, originX, originY, z);
    drawMarker(p, originX, originY, z);
    drawSim(p, originX, originY, z);
    drawHudText(p);
}

void MapCanvas::drawTrail(QPainter &p, double ox, double oy, int z)
{
    MapWidget *o = m_o;
    if (o->m_trail.size() < 2)
        return;
    p.setPen(QPen(QColor("#ffd24d"), 2));
    bool havePrev = false;
    QPointF prev;
    for (const auto &[lat, lon] : o->m_trail) {
        const auto [px, py] = lonlatToWorldPx(lat, lon, z);
        const QPointF pt(px - ox, py - oy);
        if (havePrev)
            p.drawLine(prev, pt);
        prev = pt;
        havePrev = true;
    }
}

void MapCanvas::drawMission(QPainter &p, double ox, double oy, int z)
{
    MapWidget *o = m_o;
    const auto &wps = o->m_mission.waypoints();
    if (wps.empty())
        return;
    std::vector<QPointF> pts;
    for (const auto &wp : wps) {
        const auto [px, py] = lonlatToWorldPx(wp.lat, wp.lon, z);
        pts.emplace_back(px - ox, py - oy);
    }
    if (pts.size() >= 2) {
        p.setPen(QPen(QColor("#4c9bff"), 2, Qt::DashLine));
        for (size_t i = 1; i < pts.size(); ++i)
            p.drawLine(pts[i - 1], pts[i]);
    }
    p.setFont(QFont("Consolas", 8));
    for (size_t i = 0; i < pts.size(); ++i) {
        p.setPen(QPen(QColor("#0d1117"), 2));
        p.setBrush(QBrush(QColor("#ffd24d")));
        p.drawEllipse(pts[i], 7, 7);
        p.setPen(QPen(QColor("#0d1117")));
        p.drawText(pts[i] + QPointF(-3, 4), QString::number(i + 1));
        p.setPen(QPen(QColor("#cfe3ff")));
        p.drawText(pts[i] + QPointF(10, 4), QStringLiteral("%1m").arg(wps[i].alt, 0, 'f', 0));
    }
}

void MapCanvas::drawTarget(QPainter &p, double ox, double oy, int z)
{
    MapWidget *o = m_o;
    if (!o->m_target)
        return;
    const auto [lat, lon] = *o->m_target;
    const auto [px, py] = lonlatToWorldPx(lat, lon, z);
    const double sx = px - ox, sy = py - oy;
    p.setBrush(Qt::NoBrush);
    p.setPen(QPen(QColor("#ff3df0"), 2));
    p.drawEllipse(QPointF(sx, sy), 9, 9);
    p.drawLine(QPointF(sx - 13, sy), QPointF(sx + 13, sy));
    p.drawLine(QPointF(sx, sy - 13), QPointF(sx, sy + 13));
}

void MapCanvas::drawMarker(QPainter &p, double ox, double oy, int z)
{
    MapWidget *o = m_o;
    if (!o->m_haveFix)
        return;
    const auto [px, py] = lonlatToWorldPx(o->m_lat, o->m_lon, z);
    p.save();
    p.translate(px - ox, py - oy);
    p.rotate(o->m_heading);
    p.setPen(QPen(QColor("#0d1117"), 2));
    p.setBrush(QBrush(QColor(theme::ACCENT)));
    p.drawPolygon(QPolygonF({QPointF(0, -12), QPointF(8, 9), QPointF(0, 4), QPointF(-8, 9)}));
    p.restore();
}

void MapCanvas::drawSim(QPainter &p, double ox, double oy, int z)
{
    MapWidget *o = m_o;
    if (!o->m_simPos)
        return;
    const auto &sp = *o->m_simPos;
    const auto [px, py] = lonlatToWorldPx(sp.lat, sp.lon, z);
    const double sx = px - ox, sy = py - oy;
    p.save();
    p.translate(sx, sy);
    p.rotate(sp.heading);
    p.setPen(QPen(QColor("#0d1117"), 2));
    p.setBrush(QBrush(QColor("#3fb950")));
    p.drawPolygon(QPolygonF({QPointF(0, -12), QPointF(8, 9), QPointF(0, 4), QPointF(-8, 9)}));
    p.restore();
    p.setFont(QFont("Consolas", 8));
    p.setPen(QPen(QColor("#3fb950")));
    p.drawText(QPointF(sx + 12, sy - 6), QStringLiteral("SIM %1m").arg(sp.alt, 0, 'f', 0));
}

void MapCanvas::drawHudText(QPainter &p)
{
    MapWidget *o = m_o;
    p.setFont(QFont("Consolas", 9));
    QString txt = o->m_haveFix
        ? QStringLiteral("%1, %2   z%3").arg(o->m_lat, 0, 'f', 6).arg(o->m_lon, 0, 'f', 6).arg(o->m_zoom)
        : QStringLiteral("chưa có GPS   z%1").arg(o->m_zoom);
    if (!o->m_mission.empty())
        txt += QStringLiteral("   WP%1 · %2m").arg(o->m_mission.size())
                   .arg(o->m_mission.totalLengthM(), 0, 'f', 0);
    const int tw = txt.size() * 7 + 10;
    const int x = width() - tw - 6;
    p.fillRect(x, height() - 24, tw, 18, QColor(0, 0, 0, 140));
    p.setPen(QPen(QColor(theme::TEXT)));
    p.drawText(x + 4, height() - 10, txt);
}

std::pair<double, double> MapCanvas::screenToLonlat(const QPointF &pos)
{
    MapWidget *o = m_o;
    const int z = o->m_zoom;
    const auto [cxPx, cyPx] = lonlatToWorldPx(o->m_centerLat, o->m_centerLon, z);
    const double wx = cxPx - width() / 2.0 + pos.x();
    const double wy = cyPx - height() / 2.0 + pos.y();
    return worldPxToLonlat(wx, wy, z);
}

std::optional<int> MapCanvas::wpAt(const QPointF &pos)
{
    MapWidget *o = m_o;
    const int z = o->m_zoom;
    const auto [cxPx, cyPx] = lonlatToWorldPx(o->m_centerLat, o->m_centerLon, z);
    const double ox = cxPx - width() / 2.0;
    const double oy = cyPx - height() / 2.0;
    const auto &wps = o->m_mission.waypoints();
    for (size_t i = 0; i < wps.size(); ++i) {
        const auto [px, py] = lonlatToWorldPx(wps[i].lat, wps[i].lon, z);
        if ((QPointF(px - ox, py - oy) - pos).manhattanLength() < 14)
            return int(i);
    }
    return std::nullopt;
}

void MapCanvas::contextMenuEvent(QContextMenuEvent *e)
{
    MapWidget *o = m_o;
    if (o->m_edit) {
        const auto idx = wpAt(e->pos());
        QMenu menu(this);
        QAction *setAlt = menu.addAction(QStringLiteral("Đặt độ cao waypoint…"));
        QAction *del = menu.addAction(QStringLiteral("Xoá waypoint"));
        setAlt->setEnabled(idx.has_value());
        del->setEnabled(idx.has_value());
        QAction *chosen = menu.exec(e->globalPos());
        if (idx && chosen == setAlt)
            o->askWaypointAlt(*idx);
        else if (idx && chosen == del)
            o->onWpRemoved(*idx);
        return;
    }
    const auto [lat, lon] = screenToLonlat(e->pos());
    QMenu menu(this);
    QAction *fly = menu.addAction(QStringLiteral("✈  Bay đến đây"));
    fly->setEnabled(o->m_connected);
    fly->setToolTip(QStringLiteral("Chuyển GUIDED và ra lệnh bay tới điểm này"));
    QAction *setAlt = menu.addAction(
        QStringLiteral("Đặt độ cao guided (%1 m)…").arg(o->m_guidedAlt, 0, 'f', 0));
    menu.addSeparator();
    QAction *clear = menu.addAction(QStringLiteral("Xoá mục tiêu"));
    clear->setEnabled(o->m_target.has_value());
    QAction *chosen = menu.exec(e->globalPos());
    if (chosen == fly)
        o->flyToHere(lat, lon);
    else if (chosen == setAlt)
        o->askGuidedAlt();
    else if (chosen == clear)
        o->clearTarget();
}

void MapCanvas::mousePressEvent(QMouseEvent *e)
{
    if (e->button() != Qt::LeftButton)
        return;
    MapWidget *o = m_o;
    m_press = e->position();
    m_moved = false;
    if (o->m_edit) {
        const auto idx = wpAt(e->position());
        if (idx) {
            o->m_dragWp = idx;
            return;
        }
    }
    m_dragLast = e->position();
}

void MapCanvas::mouseMoveEvent(QMouseEvent *e)
{
    MapWidget *o = m_o;
    if (m_press && (e->position() - *m_press).manhattanLength() > 4)
        m_moved = true;
    if (o->m_dragWp) {
        const auto [lat, lon] = screenToLonlat(e->position());
        o->m_mission.move(*o->m_dragWp, lat, lon);
        o->pushMission();
        return;
    }
    if (!m_dragLast)
        return;
    const QPointF delta = e->position() - *m_dragLast;
    m_dragLast = e->position();
    if (o->m_follow) {
        o->m_follow = false;
        o->m_followBtn->setChecked(false);
    }
    const int z = o->m_zoom;
    auto [cxPx, cyPx] = lonlatToWorldPx(o->m_centerLat, o->m_centerLon, z);
    cxPx -= delta.x();
    cyPx -= delta.y();
    std::tie(o->m_centerLat, o->m_centerLon) = worldPxToLonlat(cxPx, cyPx, z);
    update();
}

void MapCanvas::mouseReleaseEvent(QMouseEvent *e)
{
    MapWidget *o = m_o;
    if (o->m_dragWp) {
        const int idx = *o->m_dragWp;
        o->m_dragWp.reset();
        if (!m_moved)
            o->editWaypoint(idx);
    } else if (e->button() == Qt::LeftButton && o->m_edit && !m_moved && m_press) {
        const auto [lat, lon] = screenToLonlat(*m_press);
        o->m_mission.add(lat, lon);
        o->pushMission();
    }
    m_dragLast.reset();
    m_press.reset();
}

void MapCanvas::wheelEvent(QWheelEvent *e)
{
    MapWidget *o = m_o;
    if (o->m_edit) {
        const auto idx = wpAt(e->position());
        if (idx) {
            const bool fine = e->modifiers() & Qt::ShiftModifier;
            const double d = (fine ? 1.0 : 5.0) * (e->angleDelta().y() > 0 ? 1 : -1);
            const auto &wp = o->m_mission.waypoints()[*idx];
            o->m_mission.setAlt(*idx, std::max(0.0, wp.alt + d));
            o->pushMission();
            return;
        }
    }
    o->setZoom(o->m_zoom + (e->angleDelta().y() > 0 ? 1 : -1));
}

// ── MapWidget ────────────────────────────────────────────────────────────────
MapWidget::MapWidget(QWidget *parent) : QWidget(parent)
{
    m_provider = providers().value("Satellite");
    m_centerLat = m_lat;
    m_centerLon = m_lon;

    m_simTimer = new QTimer(this);
    connect(m_simTimer, &QTimer::timeout, this, &MapWidget::simStep);

    m_loader = new TileLoader(this);
    connect(m_loader, &TileLoader::ready, this, [this] { m_canvas->update(); });

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);
    m_controlsBar = buildControls();
    m_canvas = new MapCanvas(this);
    m_stack = new QStackedWidget(this);
    m_stack->addWidget(m_canvas); // index 0 = 2D
    outer->addWidget(m_stack, 1);
}

QWidget *MapWidget::buildControls()
{
    auto *bar = new QWidget;
    bar->setAttribute(Qt::WA_TranslucentBackground, true);
    auto *row = new QHBoxLayout(bar);
    row->setContentsMargins(0, 0, 0, 0);
    row->setSpacing(6);
    m_mapLabel = new QLabel(QStringLiteral("Bản đồ:"));
    row->addWidget(m_mapLabel);
    auto *combo = new QComboBox;
    combo->addItems(providers().keys());
    combo->setCurrentText(QStringLiteral("Satellite"));
    combo->setMaximumWidth(112);
    connect(combo, &QComboBox::currentTextChanged, this, &MapWidget::setProvider);
    row->addWidget(combo);

    m_mode3dBtn = new QPushButton(QStringLiteral("3D"));
    m_mode3dBtn->setCheckable(true);
    m_mode3dBtn->setToolTip(QStringLiteral("Chuyển giữa bản đồ 2D và quả cầu 3D"));
    connect(m_mode3dBtn, &QPushButton::clicked, this, &MapWidget::toggle3d);
    row->addWidget(m_mode3dBtn);

    m_followBtn = new QPushButton(QStringLiteral("Bám theo"));
    m_followBtn->setCheckable(true);
    m_followBtn->setChecked(true);
    m_followBtn->setToolTip(QStringLiteral("Giữ view ở giữa phương tiện"));
    connect(m_followBtn, &QPushButton::clicked, this, &MapWidget::toggleFollow);
    row->addWidget(m_followBtn);

    m_planBtn = new QPushButton(QStringLiteral("Kế hoạch"));
    m_planBtn->setToolTip(QStringLiteral("Nhiệm vụ waypoint & mô phỏng bay"));
    auto *planMenu = new QMenu(m_planBtn);

    m_editAction = planMenu->addAction(QStringLiteral("✎  Sửa waypoint"));
    m_editAction->setCheckable(true);
    m_editAction->setToolTip(QStringLiteral("Chạm chỗ trống để thêm waypoint, chạm "
        "waypoint để sửa độ cao hoặc xoá, kéo để di chuyển"));
    connect(m_editAction, &QAction::toggled, this, &MapWidget::onEditToggled);

    QAction *clearAction = planMenu->addAction(QStringLiteral("🗑  Xoá nhiệm vụ"));
    connect(clearAction, &QAction::triggered, this, &MapWidget::clearMission);

    planMenu->addSeparator();

    m_simAction = planMenu->addAction(QStringLiteral("▶  Mô phỏng tuyến"));
    m_simAction->setCheckable(true);
    m_simAction->setToolTip(QStringLiteral("Bay dấu dọc tuyến (chỉ xem trước, không lệnh gì)"));
    connect(m_simAction, &QAction::toggled, this, &MapWidget::onSimToggled);

    auto *speedW = new QWidget;
    auto *speedRow = new QHBoxLayout(speedW);
    speedRow->setContentsMargins(24, 2, 10, 4);
    speedRow->setSpacing(6);
    speedRow->addWidget(new QLabel(QStringLiteral("Tốc độ")));
    m_speedCombo = new QComboBox;
    m_speedCombo->addItems({"5 m/s", "10 m/s", "20 m/s", "40 m/s"});
    m_speedCombo->setCurrentText(QStringLiteral("10 m/s"));
    connect(m_speedCombo, &QComboBox::currentTextChanged, this, &MapWidget::setSimSpeed);
    speedRow->addWidget(m_speedCombo, 1);
    auto *speedAction = new QWidgetAction(planMenu);
    speedAction->setDefaultWidget(speedW);
    planMenu->addAction(speedAction);

    planMenu->addSeparator();
    m_uploadAction = planMenu->addAction(QStringLiteral("⬆  Tải lên phương tiện"));
    m_uploadAction->setToolTip(QStringLiteral("Gửi các waypoint đã lập cho phương tiện thành nhiệm vụ AUTO"));
    connect(m_uploadAction, &QAction::triggered, this, &MapWidget::onUpload);
    m_startAction = planMenu->addAction(QStringLiteral("➤  Bắt đầu nhiệm vụ (AUTO)"));
    m_startAction->setToolTip(QStringLiteral("Chuyển phương tiện sang AUTO và chạy nhiệm vụ đã tải"));
    connect(m_startAction, &QAction::triggered, this, &MapWidget::missionStartRequested);
    m_uploadAction->setEnabled(false);
    m_startAction->setEnabled(false);

    m_planBtn->setMenu(planMenu);
    row->addWidget(m_planBtn);

    auto *minus = new QPushButton(QStringLiteral("−"));
    minus->setObjectName("IconButton");
    connect(minus, &QPushButton::clicked, this, [this] { setZoom(m_zoom - 1); });
    auto *plus = new QPushButton(QStringLiteral("+"));
    plus->setObjectName("IconButton");
    connect(plus, &QPushButton::clicked, this, [this] { setZoom(m_zoom + 1); });
    row->addWidget(minus);
    row->addWidget(plus);
    return bar;
}

void MapWidget::setControlsCompact(bool compact)
{
    m_mapLabel->setVisible(!compact);
}

void MapWidget::updateFrom(const domain::TelemetrySnapshot &s)
{
    if (s.position.valid) {
        m_lat = s.position.lat;
        m_lon = s.position.lon;
        m_altRel = s.position.altRel;
        m_heading = s.position.headingDeg;
        if (!m_haveFix) {
            m_haveFix = true;
            m_centerLat = m_lat;
            m_centerLon = m_lon;
        }
        if (m_trail.empty() || dist(m_trail.back(), {m_lat, m_lon}) > 1e-6) {
            m_trail.emplace_back(m_lat, m_lon);
            if (m_trail.size() > 400)
                m_trail.erase(m_trail.begin());
        }
        if (m_follow) {
            m_centerLat = m_lat;
            m_centerLon = m_lon;
        }
    }
    if (m_cesium)
        m_cesium->setVehicle(m_lat, m_lon, m_altRel, m_heading, m_haveFix);
    m_canvas->update();
}

double MapWidget::dist(std::pair<double, double> a, std::pair<double, double> b)
{
    return std::abs(a.first - b.first) + std::abs(a.second - b.second);
}

void MapWidget::setProvider(const QString &name)
{
    m_provider = providers().value(name, m_provider);
    m_canvas->update();
}

void MapWidget::setZoom(int z)
{
    m_zoom = std::max(2, std::min(m_provider.maxZoom, z));
    m_canvas->update();
}

void MapWidget::toggleFollow()
{
    m_follow = m_followBtn->isChecked();
    if (m_follow && m_haveFix) {
        m_centerLat = m_lat;
        m_centerLon = m_lon;
    }
    if (m_cesium)
        m_cesium->setFollow(m_follow);
    m_canvas->update();
}

void MapWidget::toggle3d()
{
    m_mode3d = m_mode3dBtn->isChecked();
    if (m_mode3d && !m_cesium) {
        m_cesium = new CesiumView;
        connect(m_cesium, &CesiumView::waypointAdded, this, &MapWidget::onWpAdded);
        connect(m_cesium, &CesiumView::waypointMoved, this, &MapWidget::onWpMoved);
        connect(m_cesium, &CesiumView::waypointRemoved, this, &MapWidget::onWpRemoved);
        connect(m_cesium, &CesiumView::waypointAltChanged, this, &MapWidget::onWpAlt);
        connect(m_cesium, &CesiumView::waypointClicked, this, &MapWidget::editWaypoint);
        m_stack->addWidget(m_cesium);
        m_cesium->setEditMode(m_edit);
        m_cesium->setFollow(m_follow);
        m_cesium->setVehicle(m_lat, m_lon, m_altRel, m_heading, m_haveFix);
        pushMission();
    }
    m_stack->setCurrentWidget(m_mode3d ? static_cast<QWidget *>(m_cesium)
                                       : static_cast<QWidget *>(m_canvas));
}

void MapWidget::onEditToggled(bool on)
{
    m_edit = on;
    if (m_cesium)
        m_cesium->setEditMode(m_edit);
    m_canvas->update();
}

void MapWidget::clearMission()
{
    stopSim();
    m_mission.clear();
    pushMission();
}

void MapWidget::onWpAdded(double lat, double lon)
{
    m_mission.add(lat, lon);
    pushMission();
}

void MapWidget::onWpMoved(int idx, double lat, double lon)
{
    m_mission.move(idx, lat, lon);
    pushMission();
}

void MapWidget::onWpRemoved(int idx)
{
    m_mission.remove(idx);
    pushMission();
}

void MapWidget::onWpAlt(int idx, double delta)
{
    if (idx >= 0 && idx < m_mission.size()) {
        const double newAlt = std::max(0.0, m_mission.waypoints()[idx].alt + delta);
        m_mission.setAlt(idx, newAlt);
        pushMission();
    }
}

void MapWidget::editWaypoint(int idx)
{
    if (idx < 0 || idx >= m_mission.size())
        return;
    auto *dlg = new WaypointEditor(idx, m_mission.waypoints()[idx].alt, this);
    connect(dlg, &WaypointEditor::altChanged, this, [this, idx](double a) { setWpAlt(idx, a); });
    connect(dlg, &WaypointEditor::deleteRequested, this, [this, idx] { onWpRemoved(idx); });
    dlg->exec();
    dlg->deleteLater();
}

void MapWidget::setWpAlt(int idx, double alt)
{
    m_mission.setAlt(idx, std::max(0.0, alt));
    pushMission();
}

void MapWidget::pushMission()
{
    m_canvas->update();
    if (m_cesium)
        m_cesium->setWaypoints(m_mission.waypoints());
}

void MapWidget::setSimSpeed(const QString &text)
{
    bool ok = false;
    const double v = text.split(' ').first().toDouble(&ok);
    if (ok)
        m_simSpeed = v;
}

void MapWidget::onSimToggled(bool on)
{
    if (on)
        startSim();
    else
        stopSim();
}

void MapWidget::startSim()
{
    if (m_mission.size() < 2) {
        m_simAction->setChecked(false);
        return;
    }
    m_simActive = true;
    m_simDist = 0.0;
    m_simAction->setText(QStringLiteral("■  Dừng mô phỏng"));
    m_simTimer->start(int(1000 / kSimHz));
}

void MapWidget::stopSim()
{
    m_simActive = false;
    m_simTimer->stop();
    m_simPos.reset();
    m_simAction->setChecked(false);
    m_simAction->setText(QStringLiteral("▶  Mô phỏng tuyến"));
    if (m_cesium)
        m_cesium->clearSim();
    m_canvas->update();
}

void MapWidget::simStep()
{
    const double total = m_mission.totalLengthM();
    m_simDist += m_simSpeed * (1.0 / kSimHz);
    const auto pos = m_mission.interpolate(m_simDist);
    if (!pos) {
        stopSim();
        return;
    }
    m_simPos = pos;
    if (m_cesium)
        m_cesium->setSim(pos->lat, pos->lon, pos->alt, pos->heading);
    m_canvas->update();
    if (m_simDist >= total)
        stopSim();
}

void MapWidget::onUpload()
{
    if (m_mission.empty())
        return;
    emit missionUploadRequested(m_mission.waypoints());
}

void MapWidget::setConnected(bool connected)
{
    m_connected = connected;
    m_uploadAction->setEnabled(connected);
    m_startAction->setEnabled(connected);
}

void MapWidget::flyToHere(double lat, double lon)
{
    m_target = std::make_pair(lat, lon);
    emit flyToRequested(lat, lon, m_guidedAlt);
    m_canvas->update();
}

void MapWidget::askGuidedAlt()
{
    bool ok = false;
    const double alt = QInputDialog::getDouble(
        this, QStringLiteral("Độ cao guided"), QStringLiteral("Độ cao bay-đến (m so với home):"),
        m_guidedAlt, 1.0, 1000.0, 1, &ok);
    if (ok)
        m_guidedAlt = alt;
}

void MapWidget::askWaypointAlt(int idx)
{
    if (idx < 0 || idx >= m_mission.size())
        return;
    const double cur = m_mission.waypoints()[idx].alt;
    bool ok = false;
    const double alt = QInputDialog::getDouble(
        this, QStringLiteral("Độ cao waypoint"),
        QStringLiteral("Độ cao cho waypoint %1 (m):").arg(idx + 1),
        cur, 1.0, 1000.0, 1, &ok);
    if (ok) {
        m_mission.setAlt(idx, alt);
        pushMission();
    }
}

void MapWidget::clearTarget()
{
    m_target.reset();
    m_canvas->update();
}

} // namespace gcs::ui
