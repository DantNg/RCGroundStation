#include "ui/widgets.h"

#include "ui/theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QSizePolicy>

namespace gcs::ui {

Panel::Panel(const QString &title, QWidget *parent)
    : acrylic::AcrylicFrame(parent)
{
    setObjectName("Panel");
    m_outer = new QVBoxLayout(this);
    m_outer->setContentsMargins(10, 8, 10, 10);
    m_outer->setSpacing(6);
    if (!title.isEmpty()) {
        auto *lbl = new QLabel(title);
        lbl->setObjectName("PanelTitle");
        m_outer->addWidget(lbl);
    }
}

Stat::Stat(const QString &name, QWidget *parent)
    : QWidget(parent)
{
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(0, 0, 0, 0);
    auto *nameLbl = new QLabel(name);
    nameLbl->setStyleSheet(QStringLiteral("color: %1;").arg(theme::TEXT_DIM));
    m_value = new QLabel(QStringLiteral("—"));
    m_value->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_value->setStyleSheet(QStringLiteral("font-weight: 600;"));
    m_value->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    lay->addWidget(nameLbl);
    lay->addWidget(m_value, 1);
}

void Stat::set(const QString &text, const QString &color)
{
    m_value->setText(text);
    const QString c = color.isEmpty() ? QString::fromLatin1(theme::TEXT) : color;
    m_value->setStyleSheet(QStringLiteral("font-weight: 600; color: %1;").arg(c));
}

PipOverlay::PipOverlay(QWidget *parent)
    : QWidget(parent)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    setCursor(Qt::PointingHandCursor);
}

void PipOverlay::setCaption(const QString &text)
{
    if (text != m_caption) {
        m_caption = text;
        update();
    }
}

void PipOverlay::enterEvent(QEnterEvent *)
{
    m_hover = true;
    update();
}

void PipOverlay::leaveEvent(QEvent *)
{
    m_hover = false;
    update();
}

void PipOverlay::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && rect().contains(e->position().toPoint()))
        emit clicked();
}

void PipOverlay::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const int w = width(), h = height();
    const QColor accent(theme::ACCENT2);
    const QColor border = m_hover ? accent : QColor(255, 255, 255, 70);
    p.setPen(QPen(border, m_hover ? 2.0 : 1.5));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(1, 1, w - 2, h - 2), 10, 10);

    // huy hiệu phóng to (góc trên phải)
    const int bs = 22;
    const int bx = w - bs - 7, by = 7;
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(10, 14, 20, 200));
    p.drawRoundedRect(QRectF(bx, by, bs, bs), 6, 6);
    p.setPen(QPen(QColor("#ffffff"), 2));
    p.setFont(QFont("Segoe UI", 11, QFont::Bold));
    p.drawText(QRectF(bx, by, bs, bs), Qt::AlignCenter, QStringLiteral("⤢"));

    // chú thích (góc dưới trái)
    if (!m_caption.isEmpty()) {
        p.setFont(QFont("Segoe UI", 9, QFont::Bold));
        const int tw = std::max(46, 14 + int(m_caption.size()) * 8);
        p.setPen(Qt::NoPen);
        p.setBrush(QColor(10, 14, 20, 200));
        p.drawRoundedRect(QRectF(7, h - 26, tw, 19), 6, 6);
        p.setPen(QPen(QColor("#e6edf3")));
        p.drawText(QRectF(7, h - 26, tw, 19), Qt::AlignCenter, m_caption);
    }
}

} // namespace gcs::ui
