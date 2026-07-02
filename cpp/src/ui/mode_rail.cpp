#include "ui/mode_rail.h"

#include "domain/flight_modes.h"

#include <QButtonGroup>
#include <QPushButton>
#include <QSizePolicy>
#include <QVBoxLayout>

namespace gcs::ui {

namespace {
struct Action { const char *glyph; QString label; QString mode; bool isTakeoff; };
}

ModeRail::ModeRail(QWidget *parent) : QFrame(parent)
{
    setObjectName("ModeRail");
    setSizePolicy(QSizePolicy::Maximum, QSizePolicy::Maximum);

    // (glyph, nhãn, tên-chế-độ) — mode rỗng nghĩa là "cất cánh".
    const std::vector<Action> actions = {
        {"⬇", QStringLiteral("HẠ CÁNH"), QStringLiteral("LAND"), false},
        {"↩", QStringLiteral("VỀ"), QStringLiteral("RTL"), false},
        {"⏸", QStringLiteral("TẠM DỪNG"), QStringLiteral("LOITER"), false},
        {"▶", QStringLiteral("CẤT CÁNH"), QString(), true},
    };

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(6, 6, 6, 6);
    col->setSpacing(8);

    for (const auto &a : actions) {
        auto *btn = makeButton(QString::fromUtf8(a.glyph), a.label);
        if (a.isTakeoff) {
            connect(btn, &QPushButton::clicked, this, &ModeRail::takeoffRequested);
            btn->setToolTip(QStringLiteral("Cất cánh (arm + GUIDED, hỏi độ cao)"));
            m_actionBtn = btn;
        } else {
            btn->setCheckable(true);
            const QString mode = a.mode;
            connect(btn, &QPushButton::clicked, this, [this, mode] { emit modeRequested(mode); });
            btn->setToolTip(QStringLiteral("Chuyển sang %1").arg(mode));
            m_modeButtons.insert(mode, btn);
        }
        col->addWidget(btn);
    }

    col->addSpacing(6);

    // bộ chọn đơn/đa phương tiện (chỉ để trang trí — bản này bay một phương tiện)
    m_single = makeButton(QStringLiteral("◈"), QStringLiteral("ĐƠN"));
    m_single->setCheckable(true);
    m_single->setChecked(true);
    m_multi = makeButton(QStringLiteral("⧉"), QStringLiteral("ĐA"));
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

QPushButton *ModeRail::makeButton(const QString &glyph, const QString &label)
{
    auto *btn = new QPushButton(glyph + QStringLiteral("\n") + label);
    btn->setObjectName("RailBtn");
    btn->setCursor(Qt::PointingHandCursor);
    btn->setFixedSize(62, 56);
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
