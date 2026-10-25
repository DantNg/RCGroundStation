// Liên kết UART tới tay điều khiển RC (STM32F722) — bản C++ của pi_rc_link.py.
//
// STM32 gửi frame RC_STATE (cần gạt + công tắc + kênh CRSF) mỗi 20 ms qua
// /dev/ttyAMA2 (115200 8N1). Frame hai chiều: [0xAA][CMD][LEN][DATA×LEN][XOR].
// Hằng số giao thức khớp Core/Inc/comm_proto.h — KHÔNG tự đổi (xem
// PI_UI_AGENT_GUIDE.md §7–8).
//
// QSerialPort chạy theo sự kiện ngay trên luồng UI (không có luồng đọc riêng),
// nên đọc state() từ QML/timer là an toàn. Lệnh CONFIG_* được xếp hàng, mỗi lần
// một lệnh chờ ACK (timeout 0.5 s, SAVE 2 s) và trả kết quả qua callback.
#pragma once

#include <QByteArray>
#include <QObject>
#include <QString>

#include <array>
#include <deque>
#include <functional>
#include <optional>
#include <utility>
#include <vector>

class QSerialPort;
class QTimer;

namespace gcs::rc {

constexpr quint8 kHeader = 0xAA;
constexpr quint8 kModeMavlink = 0x01;
constexpr quint8 kModeRcCalib = 0x02;
constexpr quint8 kModeRcState = 0x03;
constexpr quint8 kCmdConfigWrite = 0x10;
constexpr quint8 kCmdConfigRead = 0x11;
constexpr quint8 kCmdConfigSave = 0x12;
constexpr quint8 kCmdConfigDefaults = 0x13; // [keep_cal 0/1]
constexpr quint8 kCmdConfigReload = 0x14;   // RAM ← flash (bỏ thay đổi chưa lưu)
constexpr quint8 kCmdCalStart = 0x15;       // [mask] (0 = tất cả)
constexpr quint8 kCmdCalCenter = 0x16;      // [mask]
constexpr quint8 kCmdCalApply = 0x17;       // [save 0/1] → ACK [.., status, mask]
constexpr quint8 kCmdCalCancel = 0x18;
constexpr quint8 kCmdCalStatus = 0x19;      // → ACK [.., status, active, range, center, 6×(min,mid,max)]
constexpr quint8 kCmdAck = 0x30;

// sub-id cấu hình (§6)
constexpr quint8 kSubCal = 0x01;
constexpr quint8 kSubReverse = 0x05;
constexpr quint8 kSubMap = 0x10;
constexpr quint8 kSubElrsOut = 0x23;    // [0/1] toàn bộ RC ra radio (tắt = drone failsafe)
constexpr quint8 kSubStickCrsf = 0x30;  // [0/1] cần lái → mLRS (0 = giá trị giữ, Pi điều khiển)
constexpr quint8 kSubStickHold = 0x31;  // [src 0..3][u16 µs] giá trị gửi khi STICK_CRSF = 0
constexpr quint8 kSubStickTrim = 0x32;  // [0/1] 1 = trim cố định kiểu cũ, 0 = calib 3 điểm

// mặt nạ kênh hiệu chỉnh (bit = LH,LV,RH,RV,SB,SC)
constexpr quint8 kCalMaskSticks = 0x0F;
constexpr quint8 kCalMaskSwitches = 0x30;
constexpr quint8 kCalMaskAll = 0x3F;

constexpr int kStatusRange = 5;

constexpr int kStatusOk = 0;
constexpr int kStatusTimeout = -1; // không có ACK

// Thứ tự trường trong frame.
inline constexpr std::array<const char *, 8> kControls = {"LH", "LV", "RH", "RV", "SA", "SB", "SC", "SD"};
inline constexpr std::array<const char *, 4> kSwitches = {"SA", "SB", "SC", "SD"};
inline constexpr std::array<const char *, 6> kAdcChannels = {"LH", "LV", "RH", "RV", "SB", "SC"};

QString statusName(int status);

// Một frame RC_STATE đã giải mã.
struct RcState {
    int seq = 0;
    std::array<int, 8> controlsUs{}; // LH,LV,RH,RV,SA,SB,SC,SD (µs, đã calib)
    std::array<int, 8> channelsUs{}; // CH1..CH8 gửi tới module RF
    std::array<int, 6> rawAdc{};     // LH,LV,RH,RV,SB,SC (0..4095)
    std::array<int, 4> switchPos{};  // SA..SD: 0=LOW 1=MID 2=HIGH
    bool calibLoaded = false;
    bool elrsOutput = false;
    bool stickCrsf = true;   // cần lái → drone qua mLRS (false = giao cho Pi)
    bool calActive = false;  // STM32 đang mở phiên hiệu chỉnh
    bool dirty = false;      // RAM STM32 có thay đổi chưa lưu flash
    qint64 timestampMs = 0;          // QDateTime/steady ms lúc nhận
};

// Giải mã payload RC_STATE (48 byte, big-endian); nullopt nếu sai version/độ dài.
std::optional<RcState> decodeRcState(const QByteArray &data);

QByteArray buildFrame(quint8 cmd, const QByteArray &data = {});

// Bộ tách frame theo byte; tự đồng bộ lại trên header 0xAA sau lỗi.
class FrameParser {
public:
    std::vector<std::pair<quint8, QByteArray>> feed(const QByteArray &chunk);

private:
    QByteArray m_buf;
};

class RcLink : public QObject {
    Q_OBJECT

public:
    using AckCallback = std::function<void(int status, const QByteArray &value)>;

    explicit RcLink(QObject *parent = nullptr);
    ~RcLink() override;

    // Mở cổng và giữ mở: nếu mất cổng/chưa có thiết bị thì thử lại mỗi 2 s.
    void start(const QString &portName, int baud = 115200);
    void stop();

    QString portName() const { return m_portName; }
    bool isOpen() const;
    QString lastError() const { return m_lastError; }

    const std::optional<RcState> &state() const { return m_state; }
    bool connected() const; // có frame trong 0.5 s gần nhất
    quint64 frames() const { return m_frames; }
    quint64 dropped() const { return m_dropped; }
    quint64 badFrames() const { return m_badFrames; }

    // ── lệnh (bất đồng bộ, xếp hàng) ────────────────────────────────────────
    // Callback nhận status và phần ACK sau byte status (configRead bỏ thêm byte sub).
    void command(quint8 cmd, const QByteArray &data, int timeoutMs, AckCallback cb);
    void configRead(quint8 sub, std::optional<quint8> idx, AckCallback cb);
    void configWrite(quint8 sub, const QByteArray &payload, AckCallback cb);
    void configSave(AckCallback cb);

signals:
    void stateReceived();
    void openChanged(bool open);

private:
    struct Request {
        quint8 cmd;
        QByteArray data;
        int timeoutMs;
        AckCallback cb;
    };

    void tryOpen();
    void closePort(const QString &reason);
    void closeNow();
    void onReadyRead();
    void handleFrame(quint8 cmd, const QByteArray &data);
    void send(quint8 cmd, const QByteArray &data = {});
    void enqueue(Request req);
    void pump();
    void finishCurrent(int status, const QByteArray &value);

    QSerialPort *m_port = nullptr;
    QTimer *m_reopenTimer = nullptr;
    QTimer *m_ackTimer = nullptr;
    QString m_portName;
    int m_baud = 115200;
    QString m_lastError;

    FrameParser m_parser;
    std::optional<RcState> m_state;
    std::optional<int> m_lastSeq;
    quint64 m_frames = 0;
    quint64 m_dropped = 0;
    quint64 m_badFrames = 0;

    std::deque<Request> m_queue;
    std::optional<Request> m_inFlight;
};

} // namespace gcs::rc
