// Hộp thoại thân thiện cảm ứng để sửa độ cao một waypoint (hoặc xoá nó).
//
// Chạm một waypoint trên bản đồ 2D hoặc quả cầu 3D sẽ mở hộp thoại modal nhỏ
// này. Các nút −/+ to và ô số hoạt động tốt bằng ngón tay trên màn cảm ứng. Nó
// sửa trực tiếp: mỗi thay đổi phát ``altChanged`` để bản đồ cập nhật bên dưới.
#pragma once

#include <QDialog>

class QDoubleSpinBox;

namespace gcs::ui {

class WaypointEditor : public QDialog {
    Q_OBJECT
public:
    WaypointEditor(int idx, double alt, QWidget *parent = nullptr);

signals:
    void altChanged(double alt);
    void deleteRequested();

private:
    void onDelete();
    QDoubleSpinBox *m_spin;
};

} // namespace gcs::ui
