// Một khung chứa thả nổi các con lên một nền phủ toàn màn, do caller làm chủ.
//
// Dùng cho fly-view: một view chính (bản đồ *hoặc* camera) phủ đầy sân khấu,
// trong khi HUD tròn, bảng điều khiển, nhật ký, ô PIP góc và chip kết nối nổi
// lên trên. Sân khấu giữ "ngu": chỉ đổi cha các widget được giao và, mỗi lần
// đổi kích thước, trao kích thước cho callback bố cục để chủ (MainWindow) quyết
// định vị trí.
#pragma once

#include <QWidget>

#include <functional>

namespace gcs::ui {

class OverlayStage : public QWidget {
    Q_OBJECT
public:
    explicit OverlayStage(std::function<void(int, int)> onLayout, QWidget *parent = nullptr);

    QWidget *add(QWidget *widget); // đổi cha widget lên sân khấu (thứ tự sau-trước)

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    std::function<void(int, int)> m_onLayout;
};

} // namespace gcs::ui
