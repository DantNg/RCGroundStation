#include "mavlink/decoder.h"

#include "app/store.h"

#include <QString>
#include <cstring>

namespace gcs::mavlink {

using domain::Severity;
using domain::StatusText;
using domain::TelemetrySnapshot;
using domain::nowMs;

namespace {
constexpr int kSafetyArmed = 0x80; // MAV_MODE_FLAG_SAFETY_ARMED
constexpr int kGcsSystem = 255;    // sysid của trạm mặt đất này (khớp MavlinkLink)

// MAV_RESULT → (chữ, mức độ) cho phản hồi COMMAND_ACK.
struct ResultInfo { QString text; Severity sev; };
ResultInfo resultInfo(int result)
{
    switch (result) {
    case 0: return {QStringLiteral("ĐÃ CHẤP NHẬN"), Severity::Info};
    case 1: return {QStringLiteral("TẠM THỜI TỪ CHỐI"), Severity::Warning};
    case 2: return {QStringLiteral("BỊ TỪ CHỐI"), Severity::Error};
    case 3: return {QStringLiteral("KHÔNG HỖ TRỢ"), Severity::Error};
    case 4: return {QStringLiteral("THẤT BẠI"), Severity::Error};
    case 5: return {QStringLiteral("ĐANG XỬ LÝ"), Severity::Notice};
    case 6: return {QStringLiteral("ĐÃ HUỶ"), Severity::Warning};
    default: return {QStringLiteral("KẾT QUẢ %1").arg(result), Severity::Warning};
    }
}

QString commandName(int cmd)
{
    switch (cmd) {
    case MAV_CMD_COMPONENT_ARM_DISARM: return QStringLiteral("ARM_DISARM");
    case MAV_CMD_NAV_TAKEOFF:          return QStringLiteral("NAV_TAKEOFF");
    case MAV_CMD_DO_SET_MODE:          return QStringLiteral("DO_SET_MODE");
    case MAV_CMD_MISSION_START:        return QStringLiteral("MISSION_START");
    case MAV_CMD_SET_MESSAGE_INTERVAL: return QStringLiteral("SET_MESSAGE_INTERVAL");
    default: return QStringLiteral("CMD %1").arg(cmd);
    }
}
}

TelemetryDecoder::TelemetryDecoder(app::TelemetryStore *store, NoticeSink onNotice)
    : m_store(store), m_onNotice(std::move(onNotice))
{
    if (!m_onNotice)
        m_onNotice = [](const StatusText &) {};
}

void TelemetryDecoder::handle(const MavMessage &msg)
{
    const int64_t now = nowMs();
    switch (msg.msgid) {
    case MAVLINK_MSG_ID_HEARTBEAT: {
        mavlink_heartbeat_t m;
        mavlink_msg_heartbeat_decode(&msg, &m);
        // Chỉ lấy heartbeat của bộ điều khiển bay thật. Trên một phương tiện có
        // nhiều thành phần (gimbal, camera, máy tính đồng hành, radio, GCS) cùng
        // phát heartbeat, nhưng các thành phần này để autopilot = INVALID và
        // base_mode/custom_mode = 0. Nếu nhận cả chúng thì trạng thái arm/mode
        // sẽ nhảy qua lại mỗi khi một heartbeat "không phải autopilot" tới.
        if (m.autopilot == MAV_AUTOPILOT_INVALID || m.type == MAV_TYPE_GCS)
            break;
        const bool armed = (m.base_mode & kSafetyArmed) != 0;
        m_store->mutate([&](TelemetrySnapshot &s) {
            s.mode.baseMode = m.base_mode;
            s.mode.customMode = m.custom_mode;
            s.mode.mavType = m.type;
            s.mode.autopilot = m.autopilot;
            s.mode.systemStatus = m.system_status;
            s.mode.armed = armed;
            s.mode.updatedMs = now;
            s.heartbeatSeen = true;
            s.lastHeartbeatMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_ATTITUDE: {
        mavlink_attitude_t m;
        mavlink_msg_attitude_decode(&msg, &m);
        m_store->mutate([&](TelemetrySnapshot &s) {
            s.attitude.roll = m.roll;
            s.attitude.pitch = m.pitch;
            s.attitude.yaw = m.yaw;
            s.attitude.updatedMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_SYS_STATUS: {
        mavlink_sys_status_t m;
        mavlink_msg_sys_status_decode(&msg, &m);
        const double volt = m.voltage_battery != 0xFFFF ? m.voltage_battery / 1000.0 : 0.0;
        const double cur = m.current_battery < 0 ? 0.0 : m.current_battery / 100.0;
        m_store->mutate([&](TelemetrySnapshot &s) {
            s.battery.voltage = volt;
            s.battery.current = cur;
            s.battery.remaining = m.battery_remaining;
            s.battery.updatedMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_BATTERY_STATUS: {
        mavlink_battery_status_t m;
        mavlink_msg_battery_status_decode(&msg, &m);
        const int rem = m.battery_remaining;
        m_store->mutate([&](TelemetrySnapshot &s) {
            if (rem >= 0)
                s.battery.remaining = rem;
            s.battery.updatedMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_GPS_RAW_INT: {
        mavlink_gps_raw_int_t m;
        mavlink_msg_gps_raw_int_decode(&msg, &m);
        const double hdop = m.eph == 0xFFFF ? 0.0 : m.eph / 100.0;
        m_store->mutate([&](TelemetrySnapshot &s) {
            s.gps.fixType = m.fix_type;
            s.gps.satellites = m.satellites_visible;
            s.gps.hdop = hdop;
            s.gps.updatedMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_GLOBAL_POSITION_INT: {
        mavlink_global_position_int_t m;
        mavlink_msg_global_position_int_decode(&msg, &m);
        const double lat = m.lat / 1e7;
        const double lon = m.lon / 1e7;
        m_store->mutate([&](TelemetrySnapshot &s) {
            s.position.lat = lat;
            s.position.lon = lon;
            s.position.altMsl = m.alt / 1000.0;
            s.position.altRel = m.relative_alt / 1000.0;
            if (m.hdg != 0xFFFF)
                s.position.headingDeg = m.hdg / 100.0;
            s.position.valid = (lat != 0.0 || lon != 0.0);
            s.position.updatedMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_VFR_HUD: {
        mavlink_vfr_hud_t m;
        mavlink_msg_vfr_hud_decode(&msg, &m);
        m_store->mutate([&](TelemetrySnapshot &s) {
            s.vfr.airspeed = m.airspeed;
            s.vfr.groundspeed = m.groundspeed;
            s.vfr.climb = m.climb;
            s.vfr.throttle = static_cast<int>(m.throttle);
            s.vfr.updatedMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_STATUSTEXT: {
        mavlink_statustext_t m;
        mavlink_msg_statustext_decode(&msg, &m);
        char buf[51];
        memcpy(buf, m.text, 50);
        buf[50] = '\0';
        Severity sev = Severity::Info;
        if (m.severity >= 0 && m.severity <= 7)
            sev = static_cast<Severity>(m.severity);
        StatusText st(sev, QString::fromLatin1(buf), now, true);
        m_store->mutate([&](TelemetrySnapshot &s) { s.status = st; });
        m_onNotice(st);
        break;
    }
    case MAVLINK_MSG_ID_SET_POSITION_TARGET_GLOBAL_INT: {
        // Không phải do phương tiện phát (phương tiện gửi POSITION_TARGET, id
        // khác) — đây là điểm waypoint một trạm cầm tay chia sẻ qua cầu nối. Ba
        // bit thấp của type_mask = 0 nghĩa là vị trí hợp lệ; khác đi (vd 0xFFFF)
        // là tín hiệu huỷ chọn.
        mavlink_set_position_target_global_int_t m;
        mavlink_msg_set_position_target_global_int_decode(&msg, &m);
        const bool valid = (m.type_mask & 0x7) == 0;
        m_store->mutate([&](TelemetrySnapshot &s) {
            s.sharedWaypoint.valid = valid;
            if (valid) {
                s.sharedWaypoint.lat = m.lat_int / 1e7;
                s.sharedWaypoint.lon = m.lon_int / 1e7;
                s.sharedWaypoint.altRel = m.alt;
            }
            s.sharedWaypoint.updatedMs = now;
        });
        break;
    }
    case MAVLINK_MSG_ID_COMMAND_ACK: {
        mavlink_command_ack_t m;
        mavlink_msg_command_ack_decode(&msg, &m);
        // Bỏ qua ack gửi cho một GCS khác trên mạng (target_system = 0 nghĩa là
        // ack dạng ngắn không ghi đích → vẫn nhận).
        if (m.target_system != 0 && m.target_system != kGcsSystem)
            break;
        // Bỏ qua ack của các lệnh "hạ tầng" mà trạm tự gửi định kỳ để xin luồng
        // telemetry (SET_MESSAGE_INTERVAL lặp mỗi ~5 s). Chúng không phải thao
        // tác người dùng; nếu autopilot trả KHÔNG HỖ TRỢ/BỊ TỪ CHỐI thì sẽ spam
        // nhật ký trạng thái.
        if (m.command == MAV_CMD_SET_MESSAGE_INTERVAL)
            break;
        const ResultInfo info = resultInfo(m.result);
        StatusText st(info.sev,
                      QStringLiteral("%1: %2").arg(commandName(m.command), info.text),
                      now, true);
        m_onNotice(st);
        break;
    }
    default:
        break;
    }
}

} // namespace gcs::mavlink
