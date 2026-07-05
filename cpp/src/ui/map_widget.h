// View bản đồ trượt với dấu drone, nhiệm vụ waypoint và công tắc 2D/3D.
//
// Bản đồ 2D là bộ vẽ ô XYZ Web-Mercator tự cài: ô được tải nền, cache ra đĩa và
// bộ nhớ, vẽ dưới dấu drone xoay theo hướng cùng vệt breadcrumb. Trên đó là một
// nhiệm vụ waypoint (thêm/kéo/xoá, mỗi điểm có độ cao riêng) và một bộ mô phỏng
// bay cục bộ bay dấu dọc tuyến với vận tốc chọn được — không cần phương tiện.
//
// Chuyển sang 3D thay canvas QPainter bằng quả cầu Cesium (CesiumView) hiển thị
// cùng nhiệm vụ. Hai view dùng chung một Mission.
#pragma once

#include "domain/mission.h"
#include "domain/telemetry.h"

#include <QHash>
#include <QImage>
#include <QSet>
#include <QWidget>

#include <optional>
#include <vector>

class QComboBox;
class QLabel;
class QNetworkAccessManager;
class QProgressDialog;
class QPushButton;
class QStackedWidget;
class QTimer;
class QAction;

namespace gcs::ui {

class CesiumView;
class MapCanvas;

struct TileProvider {
    QString name;
    QString urlTemplate; // {z}/{x}/{y}
    int maxZoom = 19;
    QString url(int z, int x, int y) const;
};

// Bộ tải ô nền dùng QNetworkAccessManager (bất đồng bộ trên vòng lặp sự kiện).
class TileLoader : public QObject {
    Q_OBJECT
public:
    explicit TileLoader(QObject *parent = nullptr);
    // Trả về ảnh nếu có sẵn (bộ nhớ/đĩa), ngược lại kích hoạt tải và trả null.
    QImage get(const TileProvider &provider, int z, int x, int y);
    // Chỉ tra bộ nhớ/đĩa, KHÔNG gọi mạng — dùng cho fallback overzoom.
    QImage cached(const TileProvider &provider, int z, int x, int y);

    // Tải trước mọi tile trong khung bao [w,s,e,n] các mức zoom [zmin,zmax] vào
    // cache đĩa (dùng để seed bản đồ offline). Bất đồng bộ, giới hạn số yêu cầu
    // đồng thời; phát prefetchProgress/prefetchDone. Gọi khi CÓ internet.
    void startPrefetch(const TileProvider &provider, double w, double s,
                       double e, double n, int zmin, int zmax);
    void cancelPrefetch();
    bool prefetching() const { return m_pfActive; }
    // Lý do lỗi mạng đầu tiên gặp trong lần prefetch gần nhất (để chẩn đoán).
    QString lastPrefetchError() const { return m_pfLastError; }

signals:
    void ready();
    void prefetchProgress(int done, int total);
    void prefetchDone(int saved, int skipped, int failed, bool canceled);

private:
    static QString keyOf(const QString &name, int z, int x, int y);
    static QString diskPath(const QString &name, int z, int x, int y);
    void requestTile(const TileProvider &provider, int z, int x, int y);
    void pumpPrefetch();

    QHash<QString, QImage> m_mem;
    QSet<QString> m_pending;
    QSet<QString> m_failed;
    QNetworkAccessManager *m_net;

    // Trạng thái prefetch offline.
    struct PrefetchJob { int z, x, y; };
    std::vector<PrefetchJob> m_pfJobs;
    TileProvider m_pfProvider;
    size_t m_pfNext = 0;
    int m_pfInflight = 0;
    int m_pfOk = 0;    // tải mới về đĩa
    int m_pfSkip = 0;  // đã có sẵn trong cache
    int m_pfFail = 0;  // lỗi (thường do mất mạng)
    QString m_pfLastError;
    bool m_pfActive = false;
};

class MapWidget : public QWidget {
    Q_OBJECT
public:
    explicit MapWidget(QWidget *parent = nullptr);

    QWidget *controlsWidget() { return m_controlsBar; }
    void setControlsCompact(bool compact);
    void updateFrom(const domain::TelemetrySnapshot &s);
    void setConnected(bool connected);

    // dùng bởi canvas
    void flyToHere(double lat, double lon);
    void askGuidedAlt();
    void askWaypointAlt(int idx);
    void clearTarget();

signals:
    void flyToRequested(double lat, double lon, double alt);
    void missionUploadRequested(const std::vector<domain::Waypoint> &waypoints);
    void missionStartRequested();

private:
    friend class MapCanvas;

    QWidget *buildControls();
    void setProvider(const QString &name);
    void setZoom(int z);
    void toggleFollow();
    void toggle3d();
    void onEditToggled(bool on);
    void clearMission();
    void onWpAdded(double lat, double lon);
    void onWpMoved(int idx, double lat, double lon);
    void onWpRemoved(int idx);
    void onWpAlt(int idx, double delta);
    void editWaypoint(int idx);
    void setWpAlt(int idx, double alt);
    void pushMission();
    void setSimSpeed(const QString &text);
    void onSimToggled(bool on);
    void startSim();
    void stopSim();
    void simStep();
    void onUpload();
    void downloadVietnam();
    void downloadCurrentView();
    void startOfflineDownload(double w, double s, double e, double n,
                              int zmin, int zmax, const QString &what);
    void onPrefetchProgress(int done, int total);
    void onPrefetchDone(int saved, int skipped, int failed, bool canceled);
    static double dist(std::pair<double, double> a, std::pair<double, double> b);

    TileProvider m_provider;
    int m_zoom = 16;
    double m_lat = 21.0285, m_lon = 105.8048;
    double m_altRel = 0.0;
    bool m_haveFix = false;
    double m_heading = 0.0;
    bool m_follow = true;
    bool m_connected = false;
    double m_guidedAlt = 30.0;
    std::optional<std::pair<double, double>> m_target;
    double m_centerLat = 21.0285, m_centerLon = 105.8048;
    std::vector<std::pair<double, double>> m_trail;

    domain::Mission m_mission;
    bool m_edit = false;
    bool m_mode3d = false;
    CesiumView *m_cesium = nullptr;
    std::optional<int> m_dragWp;
    bool m_simActive = false;
    double m_simDist = 0.0;
    double m_simSpeed = 10.0;
    std::optional<domain::RoutePoint> m_simPos;
    QTimer *m_simTimer = nullptr;

    TileLoader *m_loader = nullptr;
    QWidget *m_controlsBar = nullptr;
    QLabel *m_mapLabel = nullptr;
    QPushButton *m_mode3dBtn = nullptr;
    QPushButton *m_followBtn = nullptr;
    QPushButton *m_planBtn = nullptr;
    QAction *m_editAction = nullptr;
    QAction *m_simAction = nullptr;
    QAction *m_uploadAction = nullptr;
    QAction *m_startAction = nullptr;
    QComboBox *m_speedCombo = nullptr;
    QStackedWidget *m_stack = nullptr;
    MapCanvas *m_canvas = nullptr;
    QProgressDialog *m_offlineProgress = nullptr;
};

} // namespace gcs::ui
