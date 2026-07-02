// Nhật ký tin nhắn kiểu Mission Planner.
//
// Mọi STATUSTEXT từ phương tiện, mọi kết quả COMMAND_ACK và mọi thông báo nội
// bộ (kết nối/ngắt, gửi lệnh, lỗi) đều rơi vào đây, tô màu theo mức độ và có
// dấu thời gian.
#pragma once

#include "domain/telemetry.h"
#include "ui/widgets.h"

class QTextEdit;

namespace gcs::ui {

class MessagesPanel : public Panel {
    Q_OBJECT
public:
    explicit MessagesPanel(QWidget *parent = nullptr);
    void add(const domain::StatusText &st);
    static bool isAlert(const domain::StatusText &st);

private:
    QTextEdit *m_log;
};

} // namespace gcs::ui
