#include "ui/waypoint_editor.h"

#include <QDoubleSpinBox>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

namespace gcs::ui {

WaypointEditor::WaypointEditor(int idx, double alt, QWidget *parent)
    : QDialog(parent)
{
    setWindowTitle(QStringLiteral("Waypoint %1").arg(idx + 1));
    setModal(true);

    auto *col = new QVBoxLayout(this);
    col->setContentsMargins(16, 14, 16, 14);
    col->setSpacing(12);

    auto *title = new QLabel(QStringLiteral("Waypoint %1 — độ cao").arg(idx + 1));
    title->setObjectName("PanelTitle");
    col->addWidget(title);

    // ── nút to −  [giá trị m]  + (5 m mỗi lần chạm) ──────────────────────────
    auto *row = new QHBoxLayout;
    row->setSpacing(10);
    auto *minus = new QPushButton(QStringLiteral("−"));
    minus->setObjectName("IconButton");
    minus->setMinimumSize(56, 56);
    auto *plus = new QPushButton(QStringLiteral("+"));
    plus->setObjectName("IconButton");
    plus->setMinimumSize(56, 56);
    m_spin = new QDoubleSpinBox;
    m_spin->setRange(0.0, 1000.0);
    m_spin->setDecimals(0);
    m_spin->setSingleStep(1.0);
    m_spin->setSuffix(QStringLiteral(" m"));
    m_spin->setValue(alt);
    m_spin->setAlignment(Qt::AlignCenter);
    m_spin->setMinimumHeight(56);
    QFont f = m_spin->font();
    f.setPointSize(f.pointSize() + 6);
    f.setBold(true);
    m_spin->setFont(f);
    connect(minus, &QPushButton::clicked, this, [this] { m_spin->setValue(m_spin->value() - 5); });
    connect(plus, &QPushButton::clicked, this, [this] { m_spin->setValue(m_spin->value() + 5); });
    connect(m_spin, QOverload<double>::of(&QDoubleSpinBox::valueChanged),
            this, &WaypointEditor::altChanged);
    row->addWidget(minus);
    row->addWidget(m_spin, 1);
    row->addWidget(plus);
    col->addLayout(row);

    // ── xoá / xong ────────────────────────────────────────────────────────────
    auto *actions = new QHBoxLayout;
    actions->setSpacing(10);
    auto *del = new QPushButton(QStringLiteral("🗑  Xoá"));
    del->setObjectName("Disarm");
    del->setMinimumHeight(44);
    connect(del, &QPushButton::clicked, this, &WaypointEditor::onDelete);
    auto *done = new QPushButton(QStringLiteral("Xong"));
    done->setMinimumHeight(44);
    connect(done, &QPushButton::clicked, this, &QDialog::accept);
    actions->addWidget(del);
    actions->addWidget(done, 1);
    col->addLayout(actions);
}

void WaypointEditor::onDelete()
{
    emit deleteRequested();
    reject();
}

} // namespace gcs::ui
