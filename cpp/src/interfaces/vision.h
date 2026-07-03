// Các cổng (interface) cho pipeline thị giác — Yêu cầu 2: video VRX qua USB →
// phát hiện người → bắt bám mục tiêu.
//
// Tách theo trách nhiệm (Interface Segregation) và đảo phụ thuộc (DIP): vòng
// điều khiển chỉ biết các abstraction này, không biết đó là webcam hay VRX,
// detector là motion hay YOLO/ONNX, chính sách lái là gimbal hay bay tiếp cận.
// Đây chính là các điểm cắm cho AI trong tương lai.
#pragma once

#include "interfaces/ports.h"

#include <QImage>
#include <QPointF>
#include <QRectF>
#include <QString>

#include <cstdint>
#include <functional>
#include <optional>
#include <vector>

namespace gcs::interfaces {

// Một khung hình đã chuẩn hoá cho thị giác máy.
struct VideoFrame {
    QImage image;            // định dạng RGB (ARGB32/RGB32)
    int64_t timestampMs = 0;

    int width() const { return image.width(); }
    int height() const { return image.height(); }
    bool isValid() const { return !image.isNull(); }
};

// Một phát hiện. ``box`` dùng toạ độ CHUẨN HOÁ [0,1] theo chiều rộng/cao khung,
// nên độc lập với độ phân giải nguồn (webcam 720p hay VRX PAL đều như nhau).
struct Detection {
    QRectF box;              // x, y, w, h ∈ [0,1]
    float score = 0.f;       // độ tin cậy [0,1]
    int classId = 0;         // 0 = person (mặc định)
    QString label;

    QPointF center() const { return box.center(); }
};

// Kết quả một khung: toàn bộ phát hiện + mục tiêu đang khoá (nếu có).
struct TrackResult {
    std::vector<Detection> detections;
    std::optional<Detection> target;   // mục tiêu đang bám
    int64_t timestampMs = 0;
};

// ── Nguồn video ──────────────────────────────────────────────────────────────
// Bản webcam OS hôm nay, bản VRX-USB (UVC) mai — cùng interface. Nguồn tự chạy
// và đẩy khung qua sink; bên tiêu thụ không biết transport bên dưới.
class IVideoSource {
public:
    virtual ~IVideoSource() = default;

    using FrameSink = std::function<void(const VideoFrame &)>;

    virtual bool start() = 0;
    virtual void stop() = 0;
    virtual bool isRunning() const = 0;

    // Đăng ký nơi nhận khung. Có thể được gọi trên luồng khác luồng UI.
    virtual void setFrameSink(FrameSink sink) = 0;

    virtual QString sourceName() const = 0;
};

// ── Bộ phát hiện đối tượng ────────────────── ★ ĐIỂM CẮM AI CHÍNH ★ ──────────
// Hôm nay: motion/HOG đơn giản. Mai: YOLO/ONNX Runtime — thay implementation,
// KHÔNG đổi call site (Open/Closed + Dependency Inversion).
class IObjectDetector {
public:
    virtual ~IObjectDetector() = default;

    virtual std::vector<Detection> detect(const VideoFrame &frame) = 0;
    virtual QString name() const = 0;
};

// ── Bộ bám mục tiêu qua các khung ────────────────────────────────────────────
// Chọn một đối tượng để khoá và giữ nó qua nhiều khung (chống nhảy mục tiêu).
class ITracker {
public:
    virtual ~ITracker() = default;

    // Nhận phát hiện của khung hiện tại, trả về mục tiêu đang khoá (nếu có).
    virtual std::optional<Detection> update(const VideoFrame &frame,
                                            const std::vector<Detection> &dets) = 0;
    virtual void reset() = 0;
    virtual bool hasLock() const = 0;
};

// ── Chính sách bắt bám ───────────────────────────────────────────────────────
// Biến lỗi bám (lệch tâm khung) thành lệnh gửi qua ICommandSink. Bản mặc định
// lái gimbal (an toàn — không bay phương tiện); bản tương lai có thể là tiếp cận.
class ITrackingPolicy {
public:
    virtual ~ITrackingPolicy() = default;

    // Sinh lệnh lái đưa mục tiêu về tâm khung.
    virtual void steer(const Detection &target, ICommandSink &out) = 0;

    // Gọi khi mất mục tiêu (để dừng lái / giữ nguyên).
    virtual void onLost() = 0;
};

} // namespace gcs::interfaces
