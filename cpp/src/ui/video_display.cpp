#include "ui/video_display.h"

#include <QPainter>
#include <QPaintEvent>

#include <algorithm>

namespace gcs::ui {

VideoDisplay::VideoDisplay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_OpaquePaintEvent, true);
    setAutoFillBackground(false);
}

void VideoDisplay::setFrame(const QImage &image)
{
    m_image = image;
    update();
}

void VideoDisplay::setResult(const interfaces::TrackResult &result)
{
    m_result = result;
    update();
}

void VideoDisplay::clear()
{
    m_image = QImage();
    m_result = {};
    update();
}

QRectF VideoDisplay::drawnImageRect() const
{
    if (m_image.isNull())
        return QRectF();
    const QSize img = m_image.size();
    const QSize box = size();
    const double scale = std::min(double(box.width()) / img.width(),
                                  double(box.height()) / img.height());
    const double w = img.width() * scale;
    const double h = img.height() * scale;
    return QRectF((box.width() - w) / 2.0, (box.height() - h) / 2.0, w, h);
}

void VideoDisplay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.fillRect(rect(), QColor(5, 8, 12));
    if (m_image.isNull())
        return;

    const QRectF dst = drawnImageRect();
    p.setRenderHint(QPainter::SmoothPixmapTransform, true);
    p.drawImage(dst, m_image);

    // Chuyển box chuẩn hoá [0,1] → toạ độ pixel trong ô ảnh đã vẽ.
    auto toPix = [&](const QRectF &n) {
        return QRectF(dst.x() + n.x() * dst.width(),
                      dst.y() + n.y() * dst.height(),
                      n.width() * dst.width(),
                      n.height() * dst.height());
    };

    // Các phát hiện: viền mảnh mờ.
    p.setBrush(Qt::NoBrush);
    QPen det(QColor(120, 200, 255, 140));
    det.setWidthF(1.5);
    p.setPen(det);
    for (const interfaces::Detection &d : m_result.detections)
        p.drawRect(toPix(d.box));

    // Mục tiêu đang khoá: viền đậm + thập tự tâm + nhãn.
    if (m_result.target) {
        const QRectF box = toPix(m_result.target->box);
        QPen lock(QColor(90, 255, 160));
        lock.setWidthF(2.5);
        p.setPen(lock);
        p.drawRect(box);

        const QPointF c = box.center();
        p.drawLine(QPointF(c.x() - 10, c.y()), QPointF(c.x() + 10, c.y()));
        p.drawLine(QPointF(c.x(), c.y() - 10), QPointF(c.x(), c.y() + 10));

        p.setPen(QColor(90, 255, 160));
        p.drawText(box.topLeft() + QPointF(2, -4),
                   QStringLiteral("KHOÁ %1%")
                       .arg(int(m_result.target->score * 100)));
    }
}

} // namespace gcs::ui
