// Bộ phát hiện dựa trên chuyển động (frame-differencing) — placeholder cho
// detector AI thật.
//
// ★ ĐÂY LÀ CHỖ THAY BẰNG YOLO/ONNX SAU NÀY ★. Nó hiện thực IObjectDetector
// hoàn toàn bằng QImage (không phụ thuộc thư viện ngoài) nên build được ngay,
// đủ để trình diễn khoá + bắt bám. Khi tích hợp mô hình học sâu, chỉ cần viết
// một lớp mới hiện thực cùng IObjectDetector rồi đăng ký vào pipeline — vòng
// điều khiển và UI không đổi.
#pragma once

#include "interfaces/vision.h"

#include <QImage>

namespace gcs::vision {

class MotionDetector : public interfaces::IObjectDetector {
public:
    struct Config {
        int downWidth = 160;      // co nhỏ để tính nhanh, bất kể độ phân giải nguồn
        int downHeight = 120;
        int pixelThreshold = 24;  // chênh mức xám coi là "đổi"
        double minChangedFrac = 0.004; // % pixel đổi tối thiểu để coi là có mục tiêu
    };

    MotionDetector() = default;
    explicit MotionDetector(Config cfg) : m_cfg(cfg) {}

    std::vector<interfaces::Detection> detect(const interfaces::VideoFrame &frame) override;
    QString name() const override { return QStringLiteral("motion (placeholder AI)"); }

private:
    Config m_cfg;
    QImage m_prevGray;   // khung xám đã co nhỏ của lần trước
};

} // namespace gcs::vision
