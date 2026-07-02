#include "ui/top_bar.h"

#include "domain/flight_modes.h"
#include "ui/theme.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QPushButton>
#include <QSizePolicy>
#include <QStyle>

#include <set>

namespace gcs::ui {

namespace {
QStringList modeNames()
{
    std::set<QString> seen;
    QStringList out;
    auto push = [&](const QString &name) {
        if (seen.insert(name).second)
            out << name;
    };
    for (const auto &qm : domain::flight_modes::quickModes())
        push(qm.modeName);
    for (const auto &qm : domain::flight_modes::extraModes())
        push(qm.modeName);
    const auto &byId = domain::flight_modes::arducopter().byId;
    for (const QString &name : byId)
        push(name);
    return out;
}
}

StatusBar::StatusBar(QWidget *parent) : acrylic::AcrylicFrame(parent, 16)
{
    setObjectName("StatusBar");
    setSizePolicy(QSizePolicy::Preferred, QSizePolicy::Maximum);

    auto *row = new QHBoxLayout(this);
    row->setContentsMargins(14, 6, 12, 6);
    row->setSpacing(10);

    m_dot = new QLabel(QStringLiteral("●"));
    m_dot->setObjectName("ConnDot");
    m_dot->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(theme::WARN));
    m_vehicle = new QLabel(QStringLiteral("Phương tiện"));
    m_vehicle->setObjectName("Vehicle");
    m_tag = new QLabel(QStringLiteral("CHẾ ĐỘ BAY"));
    m_tag->setObjectName("ViewTag");
    row->addWidget(m_dot);
    row->addWidget(m_vehicle);
    row->addWidget(m_tag);
    row->addStretch(1);

    m_modePill = new QPushButton(QStringLiteral("—"));
    m_modePill->setObjectName("ModePill");
    m_modePill->setCursor(Qt::PointingHandCursor);
    m_modePill->setToolTip(QStringLiteral("Chế độ bay — nhấp để đổi"));
    auto *modeMenu = new QMenu(m_modePill);
    for (const QString &name : modeNames())
        modeMenu->addAction(name, this, [this, name] { emit modeRequested(name); });
    m_modePill->setMenu(modeMenu);

    m_armPill = new QPushButton(QStringLiteral("CHƯA ARM"));
    m_armPill->setObjectName("ArmPill");
    m_armPill->setCursor(Qt::PointingHandCursor);
    m_armPill->setToolTip(QStringLiteral("Nhấp để arm / disarm"));
    connect(m_armPill, &QPushButton::clicked, this, &StatusBar::onArmClicked);

    m_gps = chip(QStringLiteral("GPS —"));
    m_link = chip(QStringLiteral("KHÔNG LINK"));
    m_batt = chip(QStringLiteral("PIN —"));

    m_disc = new QPushButton(QStringLiteral("⏏"));
    m_disc->setObjectName("ChipDisconnect");
    m_disc->setCursor(Qt::PointingHandCursor);
    m_disc->setToolTip(QStringLiteral("Ngắt kết nối"));
    connect(m_disc, &QPushButton::clicked, this, &StatusBar::disconnectRequested);

    for (QWidget *w : {static_cast<QWidget *>(m_modePill), static_cast<QWidget *>(m_armPill),
                       static_cast<QWidget *>(m_gps), static_cast<QWidget *>(m_link),
                       static_cast<QWidget *>(m_batt), static_cast<QWidget *>(m_disc)})
        row->addWidget(w);

    setConnected(false);
}

QLabel *StatusBar::chip(const QString &text)
{
    auto *lbl = new QLabel(text);
    lbl->setObjectName("StatChip");
    return lbl;
}

void StatusBar::recolor(QLabel *lbl, const QString &text, const QString &color)
{
    lbl->setText(text);
    lbl->setStyleSheet(QStringLiteral("color: %1;").arg(color));
}

void StatusBar::setCompact(bool compact)
{
    m_tag->setVisible(!compact);
}

void StatusBar::setLabel(const QString &label)
{
    m_vehicle->setText(label.isEmpty() ? QStringLiteral("Phương tiện") : label);
}

void StatusBar::setConnected(bool connected)
{
    m_connected = connected;
    m_modePill->setEnabled(connected);
    m_armPill->setEnabled(connected);
    m_disc->setVisible(connected);
    if (!connected) {
        m_dot->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;").arg(theme::WARN));
        m_modePill->setText(QStringLiteral("—"));
        setArmed(false);
        recolor(m_gps, QStringLiteral("GPS —"), theme::TEXT_DIM);
        recolor(m_link, QStringLiteral("KHÔNG LINK"), theme::BAD);
        recolor(m_batt, QStringLiteral("PIN —"), theme::TEXT_DIM);
    }
}

void StatusBar::updateFrom(const domain::TelemetrySnapshot &s)
{
    if (!m_connected)
        return;
    const bool up = s.link.linkUp;
    m_dot->setStyleSheet(QStringLiteral("color: %1; font-size: 13px;")
                             .arg(up ? theme::GOOD : theme::WARN));

    if (s.heartbeatSeen) {
        m_modePill->setText(domain::flight_modes::modeName(s.mode.autopilot, s.mode.customMode));
        setArmed(s.mode.armed);
    }

    const int pct = s.battery.remaining;
    const QString bcol = pct < 0 ? theme::TEXT_DIM
        : pct < 20 ? theme::BAD
        : pct < 40 ? theme::WARN : theme::GOOD;
    QString btxt = QStringLiteral("%1V").arg(s.battery.voltage, 0, 'f', 1);
    if (pct >= 0)
        btxt += QStringLiteral(" · %1%").arg(pct);
    recolor(m_batt, btxt, bcol);

    const QString gcol = s.gps.fixType >= 3 ? theme::GOOD
        : s.gps.fixType == 2 ? theme::WARN : theme::BAD;
    recolor(m_gps, QStringLiteral("GPS %1 · %2").arg(s.gps.satellites).arg(s.gps.fixLabel()), gcol);

    recolor(m_link, up ? QStringLiteral("LINK") : QStringLiteral("KHÔNG LINK"),
            up ? theme::GOOD : theme::BAD);
}

void StatusBar::setArmed(bool armed)
{
    const QString cur = m_armPill->text();
    if (armed == m_armed && (cur == QLatin1String("ĐÃ ARM") || cur == QLatin1String("CHƯA ARM")))
        return;
    m_armed = armed;
    m_armPill->setText(armed ? QStringLiteral("ĐÃ ARM") : QStringLiteral("CHƯA ARM"));
    m_armPill->setObjectName(armed ? "DisarmPill" : "ArmPill");
    m_armPill->style()->unpolish(m_armPill);
    m_armPill->style()->polish(m_armPill);
}

void StatusBar::onArmClicked()
{
    if (m_armed)
        emit disarmRequested(false);
    else
        emit armRequested(false);
}

// ── ControlDock ──────────────────────────────────────────────────────────────
ControlDock::ControlDock(QWidget *parent) : acrylic::AcrylicFrame(parent)
{
    setObjectName("TopBar"); // dùng lại QSS acrylic trong suốt
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);
    m_row = new QHBoxLayout(this);
    m_row->setContentsMargins(10, 6, 10, 6);
    m_row->setSpacing(8);
}

void ControlDock::attachControls(QWidget *mapCtrl, QWidget *camCtrl)
{
    m_mapCtrl = mapCtrl;
    m_camCtrl = camCtrl;
    m_row->addWidget(mapCtrl);
    m_row->addWidget(camCtrl);
    camCtrl->setVisible(false);
}

void ControlDock::setActive(const QString &which)
{
    if (m_mapCtrl)
        m_mapCtrl->setVisible(which == QLatin1String("map"));
    if (m_camCtrl)
        m_camCtrl->setVisible(which == QLatin1String("cam"));
}

} // namespace gcs::ui
