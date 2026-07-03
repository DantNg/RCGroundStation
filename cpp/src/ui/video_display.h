// Widget hiển thị khung video + lớp phủ (overlay) kết quả bám.
//
// Vẽ khung hình giữ tỉ lệ trên nền tối, chồng box phát hiện (mảnh) và làm nổi
// mục tiêu đang khoá kèm thập tự tâm. Nhận dữ liệu đã tính sẵn từ pipeline —
// nó chỉ vẽ, không biết gì về detector/tracker.
#pragma once

#include "interfaces/vision.h"

#include <QImage>
#include <QWidget>

namespace gcs::ui {

class VideoDisplay : public QWidget {
    Q_OBJECT
public:
    explicit VideoDisplay(QWidget *parent = nullptr);

    void setFrame(const QImage &image);
    void setResult(const interfaces::TrackResult &result);
    void clear();

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    QRectF drawnImageRect() const;   // ô ảnh được vẽ (giữ tỉ lệ) trong widget

    QImage m_image;
    interfaces::TrackResult m_result;
};

} // namespace gcs::ui
