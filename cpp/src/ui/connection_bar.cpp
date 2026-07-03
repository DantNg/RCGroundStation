#include "ui/connection_bar.h"

#include "ui/theme.h"

#include <QComboBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QPushButton>
#include <QSerialPortInfo>
#include <QSpinBox>
#include <QStackedWidget>

namespace gcs::ui {

namespace {
const QStringList kBauds = {"9600", "19200", "38400", "57600", "115200", "230400", "921600"};

QStringList listSerialPorts()
{
    QStringList out;
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts())
        out << p.portName();
    return out;
}

// Biểu tượng cột sóng nhuộm theo trạng thái, thu về cỡ chỉ báo.
QPixmap signalDot(const QColor &color)
{
    return theme::tintedPixmap(QStringLiteral(":/icons/signal.png"), color)
        .scaled(18, 18, Qt::KeepAspectRatio, Qt::SmoothTransformation);
}
}

ConnectionBar::ConnectionBar(const AppConfig &cfg, QWidget *parent)
    : acrylic::AcrylicFrame(parent)
{
    setObjectName("ConnBar");
    auto *lay = new QHBoxLayout(this);
    lay->setContentsMargins(14, 8, 14, 8);
    lay->setSpacing(8);

    m_dot = new QLabel;
    m_dot->setPixmap(signalDot(QColor(theme::BAD)));
    m_dot->setToolTip(QStringLiteral("Trạng thái tín hiệu liên kết"));
    m_status = new QLabel(QStringLiteral("Chưa kết nối"));
    m_status->setStyleSheet(QStringLiteral("font-weight: 600;"));
    auto *vsep = new QFrame;
    vsep->setFrameShape(QFrame::VLine);
    vsep->setStyleSheet(QStringLiteral("color: %1;").arg(theme::PANEL_BORDER));
    lay->addWidget(m_dot);
    lay->addWidget(m_status);
    lay->addWidget(vsep);

    lay->addWidget(new QLabel(QStringLiteral("Kết nối:")));
    m_type = new QComboBox;
    m_type->addItems({"Serial", "UDP", "TCP"});
    connect(m_type, &QComboBox::currentIndexChanged, this, [this](int idx) { m_stack->setCurrentIndex(idx); });
    lay->addWidget(m_type);

    m_stack = new QStackedWidget;
    m_stack->addWidget(buildSerial());
    m_stack->addWidget(buildUdp());
    m_stack->addWidget(buildTcp());
    lay->addWidget(m_stack);
    lay->addStretch(1);

    m_connectBtn = new QPushButton(QStringLiteral("Kết nối"));
    m_connectBtn->setObjectName("Connect");
    connect(m_connectBtn, &QPushButton::clicked, this, [this] {
        if (m_connected)
            emit disconnectRequested();
        else
            emit connectRequested(currentConfig());
    });
    lay->addWidget(m_connectBtn);

    applyConfig(cfg);
}

QWidget *ConnectionBar::buildSerial()
{
    auto *w = new QWidget;
    auto *row = new QHBoxLayout(w);
    row->setContentsMargins(0, 0, 0, 0);
    m_port = new QComboBox;
    m_port->setEditable(true);
    m_port->setMinimumWidth(140);
    auto *refresh = new QPushButton(QStringLiteral("⟳"));
    refresh->setFixedWidth(34);
    refresh->setToolTip(QStringLiteral("Làm mới cổng serial"));
    connect(refresh, &QPushButton::clicked, this, &ConnectionBar::refreshPorts);
    m_baud = new QComboBox;
    m_baud->addItems(kBauds);
    m_baud->setCurrentText(QStringLiteral("57600"));
    row->addWidget(new QLabel(QStringLiteral("Cổng:")));
    row->addWidget(m_port);
    row->addWidget(refresh);
    row->addWidget(new QLabel(QStringLiteral("Baud:")));
    row->addWidget(m_baud);
    refreshPorts();
    return w;
}

QWidget *ConnectionBar::buildUdp()
{
    auto *w = new QWidget;
    auto *row = new QHBoxLayout(w);
    row->setContentsMargins(0, 0, 0, 0);
    m_udpPort = new QSpinBox;
    m_udpPort->setRange(1, 65535);
    m_udpPort->setValue(14550);
    row->addWidget(new QLabel(QStringLiteral("Cổng UDP nghe:")));
    row->addWidget(m_udpPort);
    return w;
}

QWidget *ConnectionBar::buildTcp()
{
    auto *w = new QWidget;
    auto *row = new QHBoxLayout(w);
    row->setContentsMargins(0, 0, 0, 0);
    m_tcpHost = new QLineEdit(QStringLiteral("127.0.0.1"));
    m_tcpHost->setFixedWidth(120);
    m_tcpPort = new QSpinBox;
    m_tcpPort->setRange(1, 65535);
    m_tcpPort->setValue(5760);
    row->addWidget(new QLabel(QStringLiteral("Máy chủ:")));
    row->addWidget(m_tcpHost);
    row->addWidget(new QLabel(QStringLiteral("Cổng:")));
    row->addWidget(m_tcpPort);
    return w;
}

void ConnectionBar::setConnected(bool connected)
{
    m_connected = connected;
    m_connectBtn->setText(connected ? QStringLiteral("Ngắt kết nối") : QStringLiteral("Kết nối"));
    m_type->setEnabled(!connected);
    m_stack->setEnabled(!connected);
    m_dot->setPixmap(signalDot(QColor(connected ? theme::GOOD : theme::BAD)));
    m_status->setText(connected ? QStringLiteral("Đã kết nối") : QStringLiteral("Chưa kết nối"));
}

AppConfig ConnectionBar::currentConfig() const
{
    AppConfig cfg;
    const QString kinds[] = {"serial", "udp", "tcp"};
    cfg.connectionType = kinds[m_type->currentIndex()];
    cfg.serialPort = m_port->currentText().trimmed();
    cfg.baud = m_baud->currentText().toInt();
    cfg.udpPort = m_udpPort->value();
    const QString host = m_tcpHost->text().trimmed();
    cfg.tcpHost = host.isEmpty() ? QStringLiteral("127.0.0.1") : host;
    cfg.tcpPort = m_tcpPort->value();
    return cfg;
}

void ConnectionBar::refreshPorts()
{
    const QString current = m_port->currentText();
    m_port->clear();
    m_port->addItems(listSerialPorts());
    if (!current.isEmpty())
        m_port->setCurrentText(current);
}

void ConnectionBar::applyConfig(const AppConfig &cfg)
{
    int idx = 0;
    if (cfg.connectionType == QLatin1String("udp")) idx = 1;
    else if (cfg.connectionType == QLatin1String("tcp")) idx = 2;
    m_type->setCurrentIndex(idx);
    if (!cfg.serialPort.isEmpty())
        m_port->setCurrentText(cfg.serialPort);
    m_baud->setCurrentText(QString::number(cfg.baud));
    m_udpPort->setValue(cfg.udpPort);
    m_tcpHost->setText(cfg.tcpHost);
    m_tcpPort->setValue(cfg.tcpPort);
}

} // namespace gcs::ui
