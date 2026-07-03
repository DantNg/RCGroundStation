#include "ui/mode_rail.h"

#include "domain/flight_modes.h"
#include "ui/theme.h"

#include <QButtonGroup>
#include <QSizePolicy>
#include <QToolButton>
#include <QVBoxLayout>

namespace gcs::ui {

namespace {
struct Action { const char *icon; QString label; QString mode; bool isTakeoff; };
}

ModeRail::ModeRail(QWidget *parent) : QFrame(parent)
{
    setObjectName("ModeRail");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    // (biểu tượng, nhãn, tên-chế-độ) — mode rỗng nghĩa là "cất cánh".
    const std::vector<Action> actions = {
        {":/icons/land.png", QStringLiteral("HẠ CÁNH"), QStringLiteral("LAND"), false},
        {":/icons/return.png", QStringLiteral("VỀ"), QStringLiteral("RTL"), false},
        {":/icons/pause.png", QStringLiteral("TẠM DỪNG"), QStringLiteral("LOITER"), false},
        {":/icons/takeoff.png", QStringLiteral("CẤT CÁNH"), QString(), true},
    };

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(6, 6, 6, 6);
    col->setSpacing(8);

    for (const auto &a : actions) {
        auto *btn = makeButton(theme::railIcon(QString::fromUtf8(a.icon)), QString(), a.label);
        if (a.isTakeoff) {
            connect(btn, &QToolButton::clicked, this, &ModeRail::takeoffRequested);
            btn->setToolTip(QStringLiteral("Cất cánh (arm + GUIDED, hỏi độ cao)"));
            m_actionBtn = btn;
        } else {
            btn->setCheckable(true);
            const QString mode = a.mode;
            connect(btn, &QToolButton::clicked, this, [this, mode] { emit modeRequested(mode); });
            btn->setToolTip(QStringLiteral("Chuyển sang %1").arg(mode));
            m_modeButtons.insert(mode, btn);
        }
        col->addWidget(btn);
    }

    col->addSpacing(6);

    // bộ chọn đơn/đa phương tiện (chỉ để trang trí — bản này bay một phương tiện)
    m_single = makeButton(QIcon(), QStringLiteral("◈"), QStringLiteral("ĐƠN"));
    m_single->setCheckable(true);
    m_single->setChecked(true);
    m_multi = makeButton(QIcon(), QStringLiteral("⧉"), QStringLiteral("ĐA"));
    m_multi->setCheckable(true);
    m_multi->setEnabled(false);
    m_multi->setToolTip(QStringLiteral("Điều khiển đa phương tiện không có ở bản này"));
    auto *grp = new QButtonGroup(this);
    grp->setExclusive(true);
    grp->addButton(m_single);
    grp->addButton(m_multi);
    col->addWidget(m_single);
    col->addWidget(m_multi);

    setConnected(false);
}

QToolButton *ModeRail::makeButton(const QIcon &icon, const QString &glyph, const QString &label)
{
    auto *btn = new QToolButton;
    btn->setObjectName("RailBtn");
    btn->setToolButtonStyle(Qt::ToolButtonTextUnderIcon);
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(62, 56);
    if (!icon.isNull()) {
        btn->setIcon(icon);
        btn->setIconSize(QSize(24, 24));
        btn->setText(label);
    } else {
        // không có PNG: dùng glyph text trên nhãn
        btn->setText(glyph + QStringLiteral("\n") + label);
    }
    return btn;
}

void ModeRail::setConnected(bool connected)
{
    m_connected = connected;
    for (auto *btn : m_modeButtons)
        btn->setEnabled(connected);
    m_actionBtn->setEnabled(connected);
    if (!connected)
        for (auto *btn : m_modeButtons)
            btn->setChecked(false);
}

void ModeRail::updateFrom(const domain::TelemetrySnapshot &s)
{
    const QString active = s.heartbeatSeen
        ? domain::flight_modes::modeName(s.mode.autopilot, s.mode.customMode)
        : QString();
    for (auto it = m_modeButtons.constBegin(); it != m_modeButtons.constEnd(); ++it)
        it.value()->setChecked(it.key() == active);
}

} // namespace gcs::ui
