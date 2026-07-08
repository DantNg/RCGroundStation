#include "bridge/backend.h"

#include "domain/flight_modes.h"

#include <QVariantMap>
#ifndef GCS_NO_SERIAL
#include <QSerialPortInfo>
#endif

namespace gcs::bridge {

Backend::Backend(app::GcsController *controller, AppConfig config, QObject *parent)
    : QObject(parent), m_controller(controller), m_config(std::move(config))
{
    // Áp cấu hình cầu nối đã lưu để nó có hiệu lực ngay khi link được mở.
    m_controller->setBridgeMode(m_config.bridgeEnabled, m_config.bridgePort);
}

QVariantList Backend::quickModes() const
{
    QVariantList out;
    auto append = [&out](const domain::flight_modes::QuickMode &qm) {
        QVariantMap m;
        m.insert(QStringLiteral("label"), qm.label);
        m.insert(QStringLiteral("mode"), qm.modeName);
        m.insert(QStringLiteral("desc"), qm.description);
        out.append(m);
    };
    for (const auto &qm : domain::flight_modes::quickModes())
        append(qm);
    for (const auto &qm : domain::flight_modes::extraModes())
        append(qm);
    return out;
}

QStringList Backend::serialPorts() const
{
    QStringList out;
#ifndef GCS_NO_SERIAL
    for (const QSerialPortInfo &p : QSerialPortInfo::availablePorts())
        out << p.portName();
#endif
    // Android: trả danh sách rỗng — QML ẩn tab Serial khi không có cổng.
    return out;
}

void Backend::openWith(AppConfig cfg)
{
    cfg.role = m_config.role; // giữ vai trò/quyền của bản dựng
    m_config = cfg;
    m_config.save(); // nhớ đường truyền vừa dùng
    m_controller->connect(m_config.connectionString(), m_config.baud, m_config.label());
}

void Backend::connectUdp(int port)
{
    AppConfig cfg = m_config;
    cfg.connectionType = QStringLiteral("udp");
    cfg.udpPort = port;
    openWith(cfg);
}

void Backend::connectTcp(const QString &host, int port)
{
    AppConfig cfg = m_config;
    cfg.connectionType = QStringLiteral("tcp");
    cfg.tcpHost = host.trimmed().isEmpty() ? QStringLiteral("127.0.0.1") : host.trimmed();
    cfg.tcpPort = port;
    openWith(cfg);
}

void Backend::connectSerial(const QString &portName, int baud)
{
    AppConfig cfg = m_config;
    cfg.connectionType = QStringLiteral("serial");
    cfg.serialPort = portName.trimmed();
    cfg.baud = baud;
    openWith(cfg);
}

void Backend::disconnect()
{
    m_controller->disconnect();
}

void Backend::setBridgeMode(bool enabled, int port)
{
    m_config.bridgeEnabled = enabled;
    m_config.bridgePort = port;
    m_config.save();
    m_controller->setBridgeMode(enabled, port);
    emit bridgeChanged();
}

void Backend::shareWaypoint(double lat, double lon, double altRel)
{
    m_controller->shareWaypoint(lat, lon, altRel);
}

void Backend::clearSharedWaypoint()
{
    m_controller->clearSharedWaypoint();
}

void Backend::arm(bool force) { m_controller->commands().arm(force); }
void Backend::disarm(bool force) { m_controller->commands().disarm(force); }
void Backend::setMode(const QString &modeName) { m_controller->commands().setModeByName(modeName); }
void Backend::takeoff(double altitudeM) { m_controller->commands().takeoff(altitudeM); }
void Backend::flyTo(double lat, double lon, double altRel) { m_controller->commands().flyTo(lat, lon, altRel); }
void Backend::startMission() { m_controller->commands().startMission(); }

} // namespace gcs::bridge
