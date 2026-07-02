#include "ui/overlay_stage.h"

namespace gcs::ui {

OverlayStage::OverlayStage(std::function<void(int, int)> onLayout, QWidget *parent)
    : QWidget(parent), m_onLayout(std::move(onLayout))
{
}

QWidget *OverlayStage::add(QWidget *widget)
{
    widget->setParent(this);
    return widget;
}

void OverlayStage::resizeEvent(QResizeEvent *event)
{
    if (m_onLayout)
        m_onLayout(width(), height());
    QWidget::resizeEvent(event);
}

} // namespace gcs::ui
