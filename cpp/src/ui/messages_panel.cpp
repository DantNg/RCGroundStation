#include "ui/messages_panel.h"

#include "ui/theme.h"

#include <QHBoxLayout>
#include <QPushButton>
#include <QScrollBar>
#include <QTextEdit>
#include <QTime>

namespace gcs::ui {

namespace {
constexpr int kMaxBlocks = 500;
}

MessagesPanel::MessagesPanel(QWidget *parent)
    : Panel(QStringLiteral("TIN NHẮN"), parent)
{
    auto *b = body();

    m_log = new QTextEdit;
    m_log->setReadOnly(true);
    m_log->document()->setMaximumBlockCount(kMaxBlocks);
    b->addWidget(m_log, 1);

    auto *row = new QHBoxLayout;
    row->addStretch(1);
    auto *clear = new QPushButton(QStringLiteral("Xoá"));
    connect(clear, &QPushButton::clicked, m_log, &QTextEdit::clear);
    row->addWidget(clear);
    b->addLayout(row);
}

void MessagesPanel::add(const domain::StatusText &st)
{
    const QString ts = QTime::currentTime().toString(QStringLiteral("HH:mm:ss"));
    const QString color = theme::severityColor(st.severity);
    QString sev = domain::severityLabel(st.severity).toUpper().left(4);
    const QString line = QStringLiteral(
        "<span style=\"color:%1\">[%2]</span> "
        "<span style=\"color:%3; font-weight:600\">%4</span> "
        "<span style=\"color:%3\">%5</span>")
        .arg(theme::TEXT_DIM, ts, color, sev.leftJustified(5), st.text.toHtmlEscaped());
    m_log->append(line);
    auto *bar = m_log->verticalScrollBar();
    bar->setValue(bar->maximum());
}

bool MessagesPanel::isAlert(const domain::StatusText &st)
{
    return static_cast<int>(st.severity) <= static_cast<int>(domain::Severity::Warning);
}

} // namespace gcs::ui
