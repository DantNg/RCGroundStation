// Khung kính mờ ("acrylic") — các thanh nổi kiểu DJI.
//
// Fly-view thả nổi các thanh (thanh trên, bảng điều khiển, nhật ký) trên bản đồ/
// camera đang chạy. Để dễ đọc mà không che khuất, mỗi thanh vẽ một mẫu *đã làm
// mờ* của thứ nằm sau nó, cộng lớp tối, bo góc và viền mảnh. Qt không có
// ``backdrop-filter`` nên mỗi khung tự chụp vùng ngay sau nó, làm mờ rẻ
// (thu nhỏ → phóng to mượt) và vẽ làm nền.
#pragma once

#include <QFrame>
#include <QPixmap>
#include <QTimer>

#include <functional>

namespace gcs::ui::acrylic {

// Đăng ký callable trả về widget mà các khung nên làm mờ (thường là view chính).
void setBackdropProvider(std::function<QWidget *()> provider);

// Làm mờ kiểu Gaussian rẻ: thu nhỏ rồi phóng to mượt.
QPixmap blurPixmap(const QPixmap &pm, double downscale = 0.18);

class AcrylicFrame : public QFrame {
    Q_OBJECT
public:
    explicit AcrylicFrame(QWidget *parent = nullptr, int radius = 12, bool shadow = true);

protected:
    void showEvent(QShowEvent *event) override;
    void hideEvent(QHideEvent *event) override;
    void paintEvent(QPaintEvent *event) override;

private:
    void maybeRefresh();
    int m_radius;
    QTimer m_refresh;
};

} // namespace gcs::ui::acrylic
