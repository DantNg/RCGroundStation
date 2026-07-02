// Link MAVLink hiện thực cả hai cổng, dựa trên Qt.
//
// Một adapter phủ cả serial, UDP và TCP. Việc nhận diễn ra trên luồng
// link-worker; việc gửi có thể được gọi từ luồng giao diện, nên mọi lệnh gửi
// được xếp vào một hàng đợi (outbox) và được luồng worker rút ra ghi đi — nhờ
// vậy đối tượng transport của Qt chỉ được đụng đến từ một luồng duy nhất.
#pragma once

#include "interfaces/ports.h"

#include <QByteArray>
#include <QHostAddress>
#include <QString>

#include <atomic>
#include <deque>
#include <mutex>
#include <memory>

class QIODevice;
class QUdpSocket;
class QTcpSocket;
class QSerialPort;

namespace gcs::mavlink {

class MavlinkLink : public interfaces::ITelemetryLink,
                    public interfaces::ICommandSink {
public:
    static constexpr int kGcsSystem = 255;
    static constexpr int kGcsComponent = MAV_COMP_ID_MISSIONPLANNER;

    // ``connStr`` theo kiểu pymavlink: "udpin:0.0.0.0:14550", "tcp:host:port"
    // hoặc đường dẫn thiết bị serial. ``baud`` chỉ dùng cho serial.
    MavlinkLink(QString connStr, int baud = 57600, QString label = QString());
    ~MavlinkLink() override;

    // ── ITelemetryLink ──────────────────────────────────────────────────────
    bool isOpen() const override { return m_device != nullptr; }
    void open() override;
    void close() override;
    std::optional<MavMessage> recv(double timeoutS) override;
    QString sourceName() const override { return m_label; }
    std::pair<int, int> target() const override
    {
        return {m_targetSystem.load(), m_targetComponent.load()};
    }
    void requestDataStreams(int rateHz = 12) override;

    // ── ICommandSink ────────────────────────────────────────────────────────
    void commandLong(int command, float p1, float p2, float p3, float p4,
                     float p5, float p6, float p7, int confirmation) override;
    void setMode(int baseMode, int customMode) override;
    void setPositionTargetGlobal(double lat, double lon, double altRel) override;
    void sendHeartbeat() override;
    void missionCount(int count, int missionType) override;
    void missionItemInt(const interfaces::MissionItem &item) override;

private:
    enum class Kind { Serial, Udp, Tcp };

    std::optional<MavMessage> parseFromBuffer();
    void learnTarget(const MavMessage &msg);
    void enqueue(const MavMessage &msg);
    void drainOutbox();
    std::pair<int, int> requireTarget() const; // ném nếu chưa có mục tiêu

    QString m_connStr;
    int m_baud;
    QString m_label;
    Kind m_kind = Kind::Serial;

    QIODevice *m_device = nullptr;         // trỏ tới một trong các socket dưới
    std::unique_ptr<QUdpSocket> m_udp;
    std::unique_ptr<QTcpSocket> m_tcp;
    std::unique_ptr<QSerialPort> m_serial;

    // đích UDP học được từ gói đầu tiên
    QHostAddress m_peerAddr;
    quint16 m_peerPort = 0;

    QByteArray m_rxBuf;   // byte đã đọc nhưng chưa phân tích
    int m_rxPos = 0;

    std::atomic<int> m_targetSystem{0};
    std::atomic<int> m_targetComponent{0};

    std::mutex m_outboxMutex;
    std::deque<QByteArray> m_outbox;
};

} // namespace gcs::mavlink
