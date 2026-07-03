// Pipeline thị giác — gốc lắp ráp của Yêu cầu 2, nối detector → tracker →
// policy.
//
// Sở hữu ba strategy qua interface (Dependency Inversion): hoán bất kỳ cái nào
// mà không đụng bên gọi. Thuần logic — KHÔNG #include widget nào; nhận một khung
// và (tuỳ chọn) một command sink, trả về kết quả bám để UI vẽ overlay. Có thể
// chạy trên luồng UI (detector nhẹ) hoặc chuyển sang worker riêng khi cắm AI
// nặng — chữ ký không đổi.
#pragma once

#include "interfaces/ports.h"
#include "interfaces/vision.h"

#include <memory>

namespace gcs::vision {

class VisionPipeline {
public:
    VisionPipeline(std::unique_ptr<interfaces::IObjectDetector> detector,
                   std::unique_ptr<interfaces::ITracker> tracker,
                   std::unique_ptr<interfaces::ITrackingPolicy> policy);

    // Xử lý một khung. Nếu bật bắt bám và có sink, lái mục tiêu qua ``sink``.
    interfaces::TrackResult process(const interfaces::VideoFrame &frame,
                                    interfaces::ICommandSink *sink);

    void setTrackingEnabled(bool on);
    bool trackingEnabled() const { return m_tracking; }

    interfaces::ITrackingPolicy &policy() { return *m_policy; }
    QString detectorName() const { return m_detector->name(); }

    void reset() { m_tracker->reset(); }

private:
    std::unique_ptr<interfaces::IObjectDetector> m_detector;
    std::unique_ptr<interfaces::ITracker> m_tracker;
    std::unique_ptr<interfaces::ITrackingPolicy> m_policy;
    bool m_tracking = false;
};

} // namespace gcs::vision
