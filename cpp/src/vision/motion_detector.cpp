#include "vision/motion_detector.h"

#include <algorithm>
#include <cstdlib>

namespace gcs::vision {

using interfaces::Detection;
using interfaces::VideoFrame;

std::vector<Detection> MotionDetector::detect(const VideoFrame &frame)
{
    std::vector<Detection> out;
    if (!frame.isValid())
        return out;

    const int w = m_cfg.downWidth;
    const int h = m_cfg.downHeight;

    // Co về kích thước cố định + chuyển xám 8-bit để so khung nhanh, ổn định.
    const QImage cur = frame.image
                           .scaled(w, h, Qt::IgnoreAspectRatio, Qt::FastTransformation)
                           .convertToFormat(QImage::Format_Grayscale8);

    // Khung đầu tiên (hoặc sau reset kích thước): chưa có gì để so.
    if (m_prevGray.width() != w || m_prevGray.height() != h) {
        m_prevGray = cur;
        return out;
    }

    int minX = w, minY = h, maxX = -1, maxY = -1;
    long changed = 0;

    for (int y = 0; y < h; ++y) {
        const uchar *rc = cur.constScanLine(y);
        const uchar *rp = m_prevGray.constScanLine(y);
        for (int x = 0; x < w; ++x) {
            if (std::abs(int(rc[x]) - int(rp[x])) > m_cfg.pixelThreshold) {
                ++changed;
                if (x < minX) minX = x;
                if (x > maxX) maxX = x;
                if (y < minY) minY = y;
                if (y > maxY) maxY = y;
            }
        }
    }

    m_prevGray = cur;

    const double frac = double(changed) / double(w * h);
    if (maxX < 0 || frac < m_cfg.minChangedFrac)
        return out;

    Detection d;
    d.box = QRectF(double(minX) / w, double(minY) / h,
                   double(maxX - minX + 1) / w, double(maxY - minY + 1) / h);
    d.score = float(std::clamp(frac * 12.0, 0.0, 1.0));
    d.classId = 0;
    d.label = QStringLiteral("chuyển động");
    out.push_back(d);
    return out;
}

} // namespace gcs::vision
