// Bộ bám theo trọng tâm — giữ khoá một mục tiêu qua nhiều khung.
//
// Chiến lược đơn giản, ổn định: nếu đang khoá, chọn phát hiện có trọng tâm gần
// mục tiêu cũ nhất (trong cổng khoảng cách); nếu chưa, chọn phát hiện điểm cao
// nhất. Cho phép "trôi" vài khung mất phát hiện trước khi mất khoá, tránh nhấp
// nháy. Hiện thực ITracker — thay bằng SORT/DeepSORT sau này không đổi call site.
#pragma once

#include "interfaces/vision.h"

#include <optional>

namespace gcs::vision {

class CentroidTracker : public interfaces::ITracker {
public:
    struct Config {
        double gate = 0.28;    // khoảng cách trọng tâm tối đa để coi là cùng mục tiêu (chuẩn hoá)
        float minScore = 0.15f; // điểm tối thiểu để nhận một phát hiện
        int maxMisses = 8;     // số khung mất phát hiện trước khi buông khoá
    };

    CentroidTracker() = default;
    explicit CentroidTracker(Config cfg) : m_cfg(cfg) {}

    std::optional<interfaces::Detection> update(
        const interfaces::VideoFrame &frame,
        const std::vector<interfaces::Detection> &dets) override;
    void reset() override;
    bool hasLock() const override { return m_target.has_value(); }

private:
    Config m_cfg;
    std::optional<interfaces::Detection> m_target;
    int m_misses = 0;
};

} // namespace gcs::vision
