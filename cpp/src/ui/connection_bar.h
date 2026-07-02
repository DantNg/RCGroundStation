// Thanh kết nối: chọn transport (serial / UDP / TCP) và kết nối.
//
// Phát ra một ``AppConfig`` hoàn chỉnh để MainWindow không phải biết chi tiết
// widget. Cổng serial được liệt kê bằng QSerialPortInfo.
#pragma once

#include "config.h"
#include "ui/acrylic.h"

class QComboBox;
class QLabel;
class QLineEdit;
class QSpinBox;
class QStackedWidget;
class QPushButton;

namespace gcs::ui {

class ConnectionBar : public acrylic::AcrylicFrame {
    Q_OBJECT
public:
    explicit ConnectionBar(const AppConfig &cfg, QWidget *parent = nullptr);

    void setConnected(bool connected);
    AppConfig currentConfig() const;

signals:
    void connectRequested(const AppConfig &cfg);
    void disconnectRequested();

private:
    QWidget *buildSerial();
    QWidget *buildUdp();
    QWidget *buildTcp();
    void refreshPorts();
    void applyConfig(const AppConfig &cfg);

    bool m_connected = false;
    QLabel *m_dot = nullptr;
    QLabel *m_status = nullptr;
    QComboBox *m_type = nullptr;
    QStackedWidget *m_stack = nullptr;
    QComboBox *m_port = nullptr;
    QComboBox *m_baud = nullptr;
    QSpinBox *m_udpPort = nullptr;
    QLineEdit *m_tcpHost = nullptr;
    QSpinBox *m_tcpPort = nullptr;
    QPushButton *m_connectBtn = nullptr;
};

} // namespace gcs::ui
