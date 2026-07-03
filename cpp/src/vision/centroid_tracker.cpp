#include "vision/centroid_tracker.h"

#include <cmath>
#include <limits>

namespace gcs::vision {

using interfaces::Detection;
using interfaces::VideoFrame;

namespace {
double dist(const QPointF &a, const QPointF &b)
{
    const double dx = a.x() - b.x();
    const double dy = a.y() - b.y();
    return std::sqrt(dx * dx + dy * dy);
}
}

void CentroidTracker::reset()
{
    m_target.reset();
    m_misses = 0;
}

std::optional<Detection> CentroidTracker::update(const VideoFrame &,
                                                 const std::vector<Detection> &dets)
{
    // Lọc theo điểm tin cậy tối thiểu.
    std::vector<const Detection *> cand;
    cand.reserve(dets.size());
    for (const Detection &d : dets)
        if (d.score >= m_cfg.minScore)
            cand.push_back(&d);

    if (cand.empty()) {
        // Không có phát hiện: cho trôi vài khung rồi mới buông khoá.
        if (m_target && ++m_misses > m_cfg.maxMisses)
            reset();
        return m_target;
    }

    const Detection *pick = nullptr;
    if (m_target) {
        // Đang khoá: chọn ứng viên gần nhất trong cổng khoảng cách.
        double best = std::numeric_limits<double>::max();
        const QPointF prev = m_target->center();
        for (const Detection *c : cand) {
            const double dd = dist(prev, c->center());
            if (dd < best && dd <= m_cfg.gate) {
                best = dd;
                pick = c;
            }
        }
    }
    if (!pick) {
        // Chưa khoá (hoặc không ứng viên nào trong cổng): lấy điểm cao nhất.
        for (const Detection *c : cand)
            if (!pick || c->score > pick->score)
                pick = c;
    }

    m_target = *pick;
    m_misses = 0;
    return m_target;
}

} // namespace gcs::vision
