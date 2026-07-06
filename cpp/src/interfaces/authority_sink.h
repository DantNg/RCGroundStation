// Cổng gác lệnh theo quyền — thực thi phân quyền (Yêu cầu 1).
//
// Là một Decorator quanh ``ICommandSink``: nếu vai trò không được điều khiển,
// mọi lệnh ghi bị chặn tại đây (bảo mật bằng KIẾN TRÚC, không phải bằng ẩn nút).
// Nhờ vậy một bản dựng "màn đeo" dùng chung mọi mã lệnh vẫn KHÔNG THỂ điều khiển
// phương tiện dù giao diện có lỗi. Sink thật được lấy qua provider nên không giữ
// tham chiếu cũ qua các lần kết nối lại (đồng nhất với CommandService).
#pragma once

#include "domain/roles.h"
#include "interfaces/ports.h"

#include <functional>

namespace gcs::interfaces {

class AuthorityGuardedSink : public ICommandSink {
public:
    using Provider = std::function<ICommandSink *()>;
    using DenyNotice = std::function<void()>;

    AuthorityGuardedSink(Provider inner, domain::Authority auth, DenyNotice onDeny = {});

    void setAuthority(domain::Authority auth) { m_auth = auth; }

    // ── ICommandSink ─────────────────────────────────────────────────────────
    void commandLong(int command,
                     float p1 = 0, float p2 = 0, float p3 = 0, float p4 = 0,
                     float p5 = 0, float p6 = 0, float p7 = 0,
                     int confirmation = 0) override;
    void setMode(int baseMode, int customMode) override;
    void setPositionTargetGlobal(double lat, double lon, double altRel) override;
    void sendHeartbeat() override;
    void missionCount(int count, int missionType = 0) override;
    void missionItemInt(const MissionItem &item) override;
    void sendRawFrame(const QByteArray &frame) override;

private:
    // Trả về sink thật nếu được phép ghi; ngược lại nullptr (và báo, có tiết chế).
    ICommandSink *pass();

    Provider m_inner;
    domain::Authority m_auth;
    DenyNotice m_onDeny;
    int64_t m_lastDenyMs = 0;
};

} // namespace gcs::interfaces
