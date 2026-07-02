#include "ui/acrylic.h"

#include <QGraphicsDropShadowEffect>
#include <QLinearGradient>
#include <QPainter>
#include <QPainterPath>

namespace gcs::ui::acrylic {

namespace {
constexpr double kBlurDownscale = 0.18;
constexpr int kRefreshMs = 90;
const QColor kTint(8, 12, 18, 140);
const QColor kTintSolid(18, 24, 32, 232);
const QColor kBorder(255, 255, 255, 28);

std::function<QWidget *()> g_backdropProvider;

QWidget *backdrop()
{
    return g_backdropProvider ? g_backdropProvider() : nullptr;
}
}

void setBackdropProvider(std::function<QWidget *()> provider)
{
    g_backdropProvider = std::move(provider);
}

QPixmap blurPixmap(const QPixmap &pm, double downscale)
{
    if (pm.isNull())
        return pm;
    const QImage img = pm.toImage();
    const int w = std::max(1, int(img.width() * downscale));
    const int h = std::max(1, int(img.height() * downscale));
    const QImage small = img.scaled(w, h, Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    const QImage big = small.scaled(img.width(), img.height(),
                                    Qt::IgnoreAspectRatio, Qt::SmoothTransformation);
    return QPixmap::fromImage(big);
}

AcrylicFrame::AcrylicFrame(QWidget *parent, int radius, bool shadow)
    : QFrame(parent), m_radius(radius)
{
    setAttribute(Qt::WA_TranslucentBackground, true);
    if (shadow) {
        auto *glow = new QGraphicsDropShadowEffect(this);
        glow->setBlurRadius(26);
        glow->setColor(QColor(0, 0, 0, 170));
        glow->setOffset(0, 4);
        setGraphicsEffect(glow);
    }
    m_refresh.setInterval(kRefreshMs);
    connect(&m_refresh, &QTimer::timeout, this, &AcrylicFrame::maybeRefresh);
}

void AcrylicFrame::showEvent(QShowEvent *event)
{
    QFrame::showEvent(event);
    if (g_backdropProvider)
        m_refresh.start();
}

void AcrylicFrame::hideEvent(QHideEvent *event)
{
    m_refresh.stop();
    QFrame::hideEvent(event);
}

void AcrylicFrame::maybeRefresh()
{
    if (isVisible() && backdrop() != nullptr)
        update();
}

void AcrylicFrame::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing, true);
    const QRectF rect(0.5, 0.5, width() - 1.0, height() - 1.0);
    QPainterPath path;
    path.addRoundedRect(rect, m_radius, m_radius);
    p.setClipPath(path);

    QWidget *src = backdrop();
    bool drewBackdrop = false;
    // đừng chụp một backdrop chứa chính ta (sẽ tái nhập paintEvent).
    if (src && src != this && src->isVisible() && !src->isAncestorOf(this)) {
        const QPoint topLeft = src->mapFromGlobal(mapToGlobal(QPoint(0, 0)));
        const QPixmap pm = src->grab(QRect(topLeft, size()));
        if (!pm.isNull()) {
            p.drawPixmap(this->rect(), blurPixmap(pm, kBlurDownscale));
            drewBackdrop = true;
        }
    }

    p.fillPath(path, drewBackdrop ? kTint : kTintSolid);

    QLinearGradient sheen(0, 0, 0, height());
    sheen.setColorAt(0.0, QColor(255, 255, 255, 22));
    sheen.setColorAt(0.45, QColor(255, 255, 255, 0));
    p.fillPath(path, QBrush(sheen));

    p.setClipping(false);
    p.setPen(QPen(kBorder, 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(rect, m_radius, m_radius);
}

} // namespace gcs::ui::acrylic
