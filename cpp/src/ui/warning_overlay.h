// Ngăn xếp biểu ngữ cảnh báo trên bản đồ.
//
// Hiện các cảnh báo/lỗi gần nhất (mức ≤ WARNING) dạng chip màu nổi trên bản đồ,
// để người vận hành không phải rời mắt khỏi vùng bay mà vẫn bắt được lỗi
// pre-arm hay báo động pin. Các mục mờ dần sau vài giây. Widget cho chuột đi
// xuyên qua nên không cướp thao tác "Bay đến đây" của bản đồ.
#pragma once

#include "domain/telemetry.h"

#include <QColor>
#include <QWidget>

#include <vector>

namespace gcs::ui {

class WarningOverlay : public QWidget {
    Q_OBJECT
public:
    explicit WarningOverlay(QWidget *parent = nullptr);
    void push(const domain::StatusText &st);
    void prune();
    bool hasItems() const { return !m_items.empty(); }

protected:
    void paintEvent(QPaintEvent *e) override;

private:
    struct Item { QString text; QColor color; int64_t expireMs; };
    std::vector<Item> m_items;
};

} // namespace gcs::ui
