#include "domain/mission.h"

#include <cmath>

namespace gcs::domain {

namespace {
constexpr double kPi = 3.14159265358979323846;
constexpr double kEarthR = 6371000.0; // m
constexpr double kDeg2Rad = kPi / 180.0;
constexpr double kRad2Deg = 180.0 / kPi;
}

double haversineM(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * kDeg2Rad;
    const double p2 = lat2 * kDeg2Rad;
    const double dphi = (lat2 - lat1) * kDeg2Rad;
    const double dlmb = (lon2 - lon1) * kDeg2Rad;
    const double a = std::sin(dphi / 2) * std::sin(dphi / 2)
        + std::cos(p1) * std::cos(p2) * std::sin(dlmb / 2) * std::sin(dlmb / 2);
    return 2 * kEarthR * std::asin(std::min(1.0, std::sqrt(a)));
}

double bearingDeg(double lat1, double lon1, double lat2, double lon2)
{
    const double p1 = lat1 * kDeg2Rad;
    const double p2 = lat2 * kDeg2Rad;
    const double dl = (lon2 - lon1) * kDeg2Rad;
    const double y = std::sin(dl) * std::cos(p2);
    const double x = std::cos(p1) * std::sin(p2)
        - std::sin(p1) * std::cos(p2) * std::cos(dl);
    return std::fmod(std::atan2(y, x) * kRad2Deg + 360.0, 360.0);
}

Waypoint &Mission::add(double lat, double lon, std::optional<double> alt)
{
    m_waypoints.push_back(Waypoint{lat, lon, alt.value_or(defaultAlt)});
    return m_waypoints.back();
}

void Mission::move(int idx, double lat, double lon)
{
    if (idx >= 0 && idx < size()) {
        m_waypoints[idx].lat = lat;
        m_waypoints[idx].lon = lon;
    }
}

void Mission::setAlt(int idx, double alt)
{
    if (idx >= 0 && idx < size())
        m_waypoints[idx].alt = alt;
}

void Mission::remove(int idx)
{
    if (idx >= 0 && idx < size())
        m_waypoints.erase(m_waypoints.begin() + idx);
}

void Mission::clear()
{
    m_waypoints.clear();
}

double Mission::totalLengthM() const
{
    double total = 0.0;
    for (size_t i = 1; i < m_waypoints.size(); ++i) {
        const auto &a = m_waypoints[i - 1];
        const auto &b = m_waypoints[i];
        total += haversineM(a.lat, a.lon, b.lat, b.lon);
    }
    return total;
}

std::optional<RoutePoint> Mission::interpolate(double distM) const
{
    const auto &wps = m_waypoints;
    if (wps.size() < 2)
        return std::nullopt;
    if (distM <= 0) {
        const auto &a = wps[0];
        const auto &b = wps[1];
        return RoutePoint{a.lat, a.lon, a.alt, bearingDeg(a.lat, a.lon, b.lat, b.lon)};
    }
    double travelled = 0.0;
    for (size_t i = 1; i < wps.size(); ++i) {
        const auto &a = wps[i - 1];
        const auto &b = wps[i];
        const double seg = haversineM(a.lat, a.lon, b.lat, b.lon);
        if (seg <= 0)
            continue;
        if (travelled + seg >= distM) {
            const double t = (distM - travelled) / seg;
            return RoutePoint{
                a.lat + (b.lat - a.lat) * t,
                a.lon + (b.lon - a.lon) * t,
                a.alt + (b.alt - a.alt) * t,
                bearingDeg(a.lat, a.lon, b.lat, b.lon)};
        }
        travelled += seg;
    }
    const auto &last = wps[wps.size() - 1];
    const auto &prev = wps[wps.size() - 2];
    return RoutePoint{last.lat, last.lon, last.alt,
                      bearingDeg(prev.lat, prev.lon, last.lat, last.lon)};
}

} // namespace gcs::domain
