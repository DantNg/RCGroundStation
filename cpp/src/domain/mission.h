// Mô hình nhiệm vụ waypoint — nguồn sự thật dùng chung cho bản đồ 2D và 3D.
//
// Một nhiệm vụ là danh sách waypoint có thứ tự, mỗi điểm gồm lat/lon và độ cao
// tương đối (m so với home/cất cánh). Nó không biết gì về MAVLink: các widget
// bản đồ chỉnh sửa nó, bộ mô phỏng nội bộ bay theo nó, và mission-service dịch
// nó sang các mục ``MAV_CMD_NAV_WAYPOINT``.
#pragma once

#include <vector>
#include <optional>

namespace gcs::domain {

double haversineM(double lat1, double lon1, double lat2, double lon2);
double bearingDeg(double lat1, double lon1, double lat2, double lon2);

struct Waypoint {
    double lat = 0.0;
    double lon = 0.0;
    double alt = 30.0;   // m, so với home/cất cánh
};

// Vị trí nội suy dọc tuyến: lat, lon, alt, heading.
struct RoutePoint {
    double lat;
    double lon;
    double alt;
    double heading;
};

class Mission {
public:
    // ── thay đổi ──────────────────────────────────────────────────────────────
    Waypoint &add(double lat, double lon, std::optional<double> alt = std::nullopt);
    void move(int idx, double lat, double lon);
    void setAlt(int idx, double alt);
    void remove(int idx);
    void clear();

    // ── truy vấn ──────────────────────────────────────────────────────────────
    int size() const { return static_cast<int>(m_waypoints.size()); }
    bool empty() const { return m_waypoints.empty(); }
    const std::vector<Waypoint> &waypoints() const { return m_waypoints; }
    std::vector<Waypoint> &waypoints() { return m_waypoints; }

    double totalLengthM() const;

    // Vị trí ``distM`` dọc tuyến; std::nullopt nếu ít hơn hai waypoint.
    std::optional<RoutePoint> interpolate(double distM) const;

    double defaultAlt = 30.0;

private:
    std::vector<Waypoint> m_waypoints;
};

} // namespace gcs::domain
