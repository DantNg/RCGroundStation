#include "app/store.h"

namespace gcs::app {

void TelemetryStore::mutate(const std::function<void(domain::TelemetrySnapshot &)> &fn)
{
    std::lock_guard<std::mutex> lock(m_mutex);
    fn(m_snap);
}

domain::TelemetrySnapshot TelemetryStore::snapshot() const
{
    std::lock_guard<std::mutex> lock(m_mutex);
    return m_snap; // sao chép theo giá trị (mọi thành viên là POD/QString)
}

void TelemetryStore::reset()
{
    std::lock_guard<std::mutex> lock(m_mutex);
    m_snap = domain::TelemetrySnapshot{};
}

} // namespace gcs::app
