// Dịch các tin MAVLink đã phân tích thành cập nhật cho telemetry-store.
//
// Telemetry đi vào kho dùng chung; STATUSTEXT và COMMAND_ACK được đưa ra ngoài
// dưới dạng *thông báo* qua một callback được tiêm vào, để giao diện hiển thị
// chúng trong nhật ký kiểu Mission Planner (bản thân bộ giải mã không đụng giao
// diện).
#pragma once

#include "domain/telemetry.h"
#include "mavlink/mav_message.h"

#include <functional>

namespace gcs::app { class TelemetryStore; }

namespace gcs::mavlink {

using NoticeSink = std::function<void(const domain::StatusText &)>;

class TelemetryDecoder {
public:
    explicit TelemetryDecoder(app::TelemetryStore *store, NoticeSink onNotice = {});

    void handle(const MavMessage &msg);

private:
    app::TelemetryStore *m_store;
    NoticeSink m_onNotice;
};

} // namespace gcs::mavlink
