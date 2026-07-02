// View bản đồ 3D — quả cầu Cesium chạy trong QWebEngineView.
//
// Bọc trang ``assets/cesium_map.html`` đã đóng gói và bắc cầu tới phần còn lại
// của ứng dụng (thuần Qt). C++ đẩy vị trí phương tiện, danh sách waypoint và dấu
// mô phỏng vào trang bằng cách gọi các hàm ``window.gcs*`` của nó; trang đẩy
// chỉnh sửa của người dùng trở lại qua một đối tượng QWebChannel tên ``bridge``.
//
// Cesium cần Qt WebEngine. Khi WebEngine vắng mặt (vd Qt cho MinGW trên
// Windows), lớp này biên dịch thành một placeholder giữ nguyên API để phần còn
// lại của ứng dụng không phải quan tâm.
#pragma once

#include "domain/mission.h"

#include <QStringList>
#include <vector>

#ifdef HAVE_WEBENGINE
#  include <QWebEngineView>
namespace gcs::ui { using CesiumViewBase = QWebEngineView; }
#else
#  include <QWidget>
namespace gcs::ui { using CesiumViewBase = QWidget; }
#endif

namespace gcs::ui {

class CesiumBridge; // đối tượng nhận sự kiện JS → C++

class CesiumView : public CesiumViewBase {
    Q_OBJECT
public:
    explicit CesiumView(QWidget *parent = nullptr);

    void setVehicle(double lat, double lon, double alt, double heading, bool haveFix);
    void setWaypoints(const std::vector<domain::Waypoint> &waypoints);
    void setSim(double lat, double lon, double alt, double heading);
    void clearSim();
    void setEditMode(bool on);
    void setFollow(bool on);
    void flyToVehicle();

signals:
    void waypointAdded(double lat, double lon);
    void waypointMoved(int idx, double lat, double lon);
    void waypointRemoved(int idx);
    void waypointAltChanged(int idx, double deltaM);
    void waypointClicked(int idx);

#ifdef HAVE_WEBENGINE
private:
    void onLoaded(bool ok);
    void js(const QString &code);

    bool m_ready = false;
    QStringList m_pending;
    CesiumBridge *m_bridge = nullptr;
#endif
};

} // namespace gcs::ui
