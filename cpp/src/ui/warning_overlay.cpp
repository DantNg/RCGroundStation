#include "ui/warning_overlay.h"

#include "ui/theme.h"

#include <QFontMetrics>
#include <QPainter>

namespace gcs::ui {

namespace {
constexpr int64_t kHoldMs = 9000;
constexpr size_t kMaxItems = 4;
}

WarningOverlay::WarningOverlay(QWidget *parent) : QWidget(parent)
{
    setAttribute(Qt::WA_TransparentForMouseEvents, true);
    setAttribute(Qt::WA_TranslucentBackground, true);
}

void WarningOverlay::push(const domain::StatusText &st)
{
    const int64_t expire = domain::nowMs() + kHoldMs;
    m_items.insert(m_items.begin(),
                   Item{st.text, QColor(theme::severityColor(st.severity)), expire});
    if (m_items.size() > kMaxItems)
        m_items.resize(kMaxItems);
    update();
}

void WarningOverlay::prune()
{
    const int64_t now = domain::nowMs();
    const size_t before = m_items.size();
    std::vector<Item> keep;
    for (const auto &it : m_items)
        if (it.expireMs > now)
            keep.push_back(it);
    if (keep.size() != before) {
        m_items = std::move(keep);
        update();
    }
}

void WarningOverlay::paintEvent(QPaintEvent *)
{
    if (m_items.empty())
        return;
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    p.setRenderHint(QPainter::TextAntialiasing, true);
    const int w = width();
    QFont font("Segoe UI", 11, QFont::Bold);
    p.setFont(font);
    const QFontMetrics fm(font);
    const int chipH = fm.height() + 12;
    const int gap = 6;
    int y = 0;
    for (const auto &it : m_items) {
        const QRectF rect(0, y, w, chipH);
        QColor bg = it.color;
        bg.setAlpha(225);
        p.setPen(Qt::NoPen);
        p.setBrush(QBrush(bg));
        p.drawRoundedRect(rect, 6, 6);
        const QString label = QStringLiteral("⚠  ") + fm.elidedText(it.text, Qt::ElideRight, w - 40);
        p.setPen(QPen(QColor("#ffffff")));
        p.drawText(rect.adjusted(12, 0, -10, 0), Qt::AlignVCenter | Qt::AlignLeft, label);
        y += chipH + gap;
    }
}

} // namespace gcs::ui
