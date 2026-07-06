// Chế độ cầu nối MAVLink qua Wi-Fi.
//
// Biến trạm cầm tay này thành một bộ chuyển tiếp: mỗi khung MAVLink nhận được
// từ phương tiện được phát UDP broadcast (255.255.255.255) ra một cổng GCS, nhờ
// vậy bất kỳ máy tính nào trong cùng mạng Wi-Fi (Mission Planner/QGroundControl)
// chỉ cần mở "UDP" là đọc được telemetry — không cần biết địa chỉ IP của tay
// điều khiển. Khi máy tính gửi gói trả về (COMMAND/PARAM/MISSION…), bridge tiêm
// ngược vào phương tiện qua callback ``inject`` — nhưng chỉ khi ``allowUplink``.
//
// Socket được ràng vào cổng nguồn tạm thời (ephemeral) và broadcast tới cổng
// đích cố định, nên gói broadcast của chính ta KHÔNG dội về cổng nguồn → không
// có vòng lặp phản hồi. Mọi thao tác socket chỉ diễn ra trên luồng link-worker
// (giống MavlinkLink); các setter cấu hình dùng atomic nên gọi được từ luồng
// giao diện.
#pragma once

#include "mavlink/mav_message.h"

#include <QByteArray>
#include <QString>

#include <atomic>
#include <cstdint>
#include <deque>
#include <functional>
#include <memory>
#include <mutex>

class QUdpSocket;

namespace gcs::mavlink {

class MavlinkBridge {
public:
    // Tiêm một gói thô (từ máy tính) ngược vào phương tiện.
    using InjectSink = std::function<void(const QByteArray &)>;
    // Báo trạng thái (text, isError) — chuyển tiếp về nhật ký của trạm.
    using NoticeFn = std::function<void(const QString &, bool)>;

    // Ctor/dtor định nghĩa out-of-line vì unique_ptr<QUdpSocket> cần kiểu đầy đủ.
    MavlinkBridge();
    ~MavlinkBridge();

    // ── cấu hình (an toàn-luồng, gọi từ luồng giao diện) ──────────────────────
    void configure(bool enabled, uint16_t port, bool allowUplink);
    bool enabled() const { return m_enabled.load(); }

    // ── chia sẻ điểm waypoint đang chọn (an toàn-luồng) ───────────────────────
    // Broadcast toạ độ điểm operator chọn để trạm giám sát thấy; ``clearWaypoint``
    // báo đã huỷ chọn. Không làm gì nếu cầu nối đang tắt. Byte thật được gửi ở
    // service() trên luồng worker.
    void shareWaypoint(double lat, double lon, double altRel);
    void clearWaypoint();

    // ── chỉ gọi trên luồng link-worker ────────────────────────────────────────
    // Đồng bộ socket với cờ enabled (mở/đóng khi đổi), rồi rút mọi gói máy tính
    // gửi về và (nếu cho phép uplink) tiêm vào phương tiện qua ``inject``.
    void service(const InjectSink &inject, const NoticeFn &notice);
    // Broadcast một khung vừa nhận từ phương tiện ra mạng.
    void forward(const MavMessage &msg);
    // Đóng socket (khi dừng link).
    void shutdown();

private:
    void ensureOpen(const NoticeFn &notice);
    void queueFrame(const MavMessage &msg); // đóng gói + xếp hàng broadcast (an toàn-luồng)

    std::atomic<bool> m_enabled{false};
    std::atomic<uint16_t> m_port{14550};
    std::atomic<bool> m_allowUplink{true};

    std::unique_ptr<QUdpSocket> m_socket; // chỉ luồng worker chạm vào
    bool m_bindFailed = false;            // đã báo lỗi bind — tránh spam

    // Khung do luồng giao diện xếp (chia sẻ waypoint) → worker rút ra broadcast.
    std::mutex m_txMutex;
    std::deque<QByteArray> m_txQueue;
};

} // namespace gcs::mavlink
