// Worker nền điều khiển một link telemetry.
//
// Sở hữu vòng lặp nhận trên luồng daemon riêng: đọc link → giải mã vào kho →
// duy trì thống kê sức khoẻ link → phát heartbeat GCS ~1 Hz. Mọi đầu ra liên
// luồng đi qua kho (telemetry) và một notice sink được tiêm vào (thông báo);
// worker không bao giờ đụng Qt.
#pragma once

#include "domain/telemetry.h"
#include "interfaces/ports.h"
#include "mavlink/decoder.h"
#include "mavlink/mavlink_bridge.h"

#include <atomic>
#include <functional>
#include <memory>
#include <thread>

namespace gcs::app {

class TelemetryStore;

using NoticeSink = std::function<void(const domain::StatusText &)>;
using MessageObserver = std::function<void(const MavMessage &)>;

class LinkManager {
public:
    LinkManager(TelemetryStore *store, NoticeSink noticeSink);
    ~LinkManager();

    // Đăng ký callback được nạp mỗi tin đến (vd bộ xử lý giao thức nhiệm vụ).
    // Chạy trên luồng link-worker — giữ nhanh và an toàn-luồng.
    void setObserver(MessageObserver observer);

    // Link đang hoạt động (cũng là command sink), hoặc nullptr nếu đã dừng.
    interfaces::ICommandSink *commandSink() const;
    interfaces::ITelemetryLink *link() const { return m_link.get(); }

    void start(std::unique_ptr<interfaces::ITelemetryLink> link);
    void stop();

    // ── chế độ cầu nối (chuyển tiếp MAVLink qua Wi-Fi) ────────────────────────
    // An toàn-luồng: chỉ đặt cờ; luồng worker áp dụng ở vòng lặp kế. ``allowUplink``
    // do quyền của trạm quyết định (chỉ cấp điều khiển mới cho máy tính gửi về).
    void setBridge(bool enabled, int port, bool allowUplink);
    bool bridgeEnabled() const { return m_bridge.enabled(); }

    // Chia sẻ điểm waypoint đang chọn ra mạng (nếu cầu nối bật). An toàn-luồng.
    void shareWaypoint(double lat, double lon, double altRel) { m_bridge.shareWaypoint(lat, lon, altRel); }
    void clearSharedWaypoint() { m_bridge.clearWaypoint(); }

private:
    void run();
    void writeStats(uint64_t frames, uint64_t bytes, uint64_t errors,
                    int64_t lastFrameMs, int64_t now);

    TelemetryStore *m_store;
    NoticeSink m_notice;
    mavlink::TelemetryDecoder m_decoder;
    std::unique_ptr<interfaces::ITelemetryLink> m_link;
    std::thread m_thread;
    std::atomic<bool> m_stop{false};
    MessageObserver m_observer;
    mavlink::MavlinkBridge m_bridge;
};

} // namespace gcs::app
