#include "config.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QStandardPaths>

namespace gcs {

namespace {
QString configPath()
{
    return QDir::homePath() + QStringLiteral("/.lite_gcs/config.json");
}
}

QString AppConfig::connectionString() const
{
    if (connectionType == QLatin1String("udp"))
        // Nghe bất kỳ ai phát tới ta (SITL forward, ESP32 UDP forward, bộ mô phỏng).
        return QStringLiteral("udpin:0.0.0.0:%1").arg(udpPort);
    if (connectionType == QLatin1String("tcp"))
        return QStringLiteral("tcp:%1:%2").arg(tcpHost).arg(tcpPort);
    return serialPort; // đường dẫn thiết bị serial (baud truyền riêng)
}

QString AppConfig::label() const
{
    if (connectionType == QLatin1String("udp"))
        return QStringLiteral("udp:%1").arg(udpPort);
    if (connectionType == QLatin1String("tcp"))
        return QStringLiteral("tcp:%1:%2").arg(tcpHost).arg(tcpPort);
    return serialPort.isEmpty() ? QStringLiteral("serial") : serialPort;
}

AppConfig AppConfig::load()
{
    AppConfig cfg;
    QFile f(configPath());
    if (!f.open(QIODevice::ReadOnly))
        return cfg;
    const QJsonObject o = QJsonDocument::fromJson(f.readAll()).object();
    if (o.contains("connection_type")) cfg.connectionType = o.value("connection_type").toString(cfg.connectionType);
    if (o.contains("serial_port"))     cfg.serialPort = o.value("serial_port").toString(cfg.serialPort);
    if (o.contains("baud"))            cfg.baud = o.value("baud").toInt(cfg.baud);
    if (o.contains("udp_port"))        cfg.udpPort = o.value("udp_port").toInt(cfg.udpPort);
    if (o.contains("tcp_host"))        cfg.tcpHost = o.value("tcp_host").toString(cfg.tcpHost);
    if (o.contains("tcp_port"))        cfg.tcpPort = o.value("tcp_port").toInt(cfg.tcpPort);
    if (o.contains("role"))            cfg.role = o.value("role").toString(cfg.role);
    if (o.contains("bridge_enabled"))  cfg.bridgeEnabled = o.value("bridge_enabled").toBool(cfg.bridgeEnabled);
    if (o.contains("bridge_port"))     cfg.bridgePort = o.value("bridge_port").toInt(cfg.bridgePort);
    return cfg;
}

void AppConfig::save() const
{
    QDir().mkpath(QFileInfo(configPath()).absolutePath());
    QJsonObject o;
    o["connection_type"] = connectionType;
    o["serial_port"] = serialPort;
    o["baud"] = baud;
    o["udp_port"] = udpPort;
    o["tcp_host"] = tcpHost;
    o["tcp_port"] = tcpPort;
    o["role"] = role;
    o["bridge_enabled"] = bridgeEnabled;
    o["bridge_port"] = bridgePort;
    QFile f(configPath());
    if (f.open(QIODevice::WriteOnly | QIODevice::Truncate))
        f.write(QJsonDocument(o).toJson(QJsonDocument::Indented));
}

} // namespace gcs
