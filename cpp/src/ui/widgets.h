// Các thành phần view nhỏ dùng chung giữa các bảng.
#pragma once

#include "ui/acrylic.h"

#include <QVBoxLayout>
#include <QWidget>

class QLabel;

namespace gcs::ui {

// Một khung có tiêu đề — thẻ kính mờ nổi trên fly-view.
class Panel : public acrylic::AcrylicFrame {
    Q_OBJECT
public:
    explicit Panel(const QString &title, QWidget *parent = nullptr);
    QVBoxLayout *body() { return m_outer; }

private:
    QVBoxLayout *m_outer;
};

// Một dòng nhãn + giá trị, vd ``Pin   12.4 V``.
class Stat : public QWidget {
    Q_OBJECT
public:
    explicit Stat(const QString &name, QWidget *parent = nullptr);
    void set(const QString &text, const QString &color = QString());

private:
    QLabel *m_value;
};

// Khung trong suốt có thể nhấp đặt lên ô picture-in-picture.
class PipOverlay : public QWidget {
    Q_OBJECT
public:
    explicit PipOverlay(QWidget *parent = nullptr);
    void setCaption(const QString &text);

signals:
    void clicked();

protected:
    void enterEvent(QEnterEvent *e) override;
    void leaveEvent(QEvent *e) override;
    void mouseReleaseEvent(QMouseEvent *e) override;
    void paintEvent(QPaintEvent *e) override;

private:
    bool m_hover = false;
    QString m_caption;
};

} // namespace gcs::ui
