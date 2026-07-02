// Kho telemetry an toàn-luồng — điểm bàn giao dữ liệu giữa các luồng.
//
// Luồng worker thay đổi nó dưới một khoá; giao diện đọc ra các ảnh chụp nguyên
// tử. Đây là bản desktop của ``TelemetryStore`` trong firmware (một mutex
// FreeRTOS bảo vệ một ``TelemetrySnapshot``).
#pragma once

#include "domain/telemetry.h"

#include <functional>
#include <mutex>

namespace gcs::app {

class TelemetryStore {
public:
    // Áp dụng ``fn`` lên snapshot đang sống trong khi giữ khoá.
    void mutate(const std::function<void(domain::TelemetrySnapshot &)> &fn);

    // Trả về một bản sao độc lập, an toàn để đọc trên luồng khác.
    domain::TelemetrySnapshot snapshot() const;

    void reset();

private:
    mutable std::mutex m_mutex;
    domain::TelemetrySnapshot m_snap;
};

} // namespace gcs::app
