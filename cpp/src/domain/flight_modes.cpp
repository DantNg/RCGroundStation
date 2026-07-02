#include "domain/flight_modes.h"

namespace gcs::domain::flight_modes {

std::optional<QString> ModeTable::nameOf(int customMode) const
{
    auto it = byId.find(customMode);
    if (it != byId.end())
        return it.value();
    return std::nullopt;
}

std::optional<int> ModeTable::idOf(const QString &modeName) const
{
    const QString target = modeName.trimmed().toUpper();
    for (auto it = byId.constBegin(); it != byId.constEnd(); ++it) {
        if (it.value() == target)
            return it.key();
    }
    return std::nullopt;
}

const ModeTable &arducopter()
{
    // Khớp với bảng FlightMode.h của firmware.
    static const ModeTable table = [] {
        ModeTable t;
        t.name = QStringLiteral("ArduCopter");
        t.autopilot = MAV_AUTOPILOT_ARDUPILOTMEGA;
        t.byId = {
            {0, "STABILIZE"}, {1, "ACRO"}, {2, "ALT_HOLD"}, {3, "AUTO"},
            {4, "GUIDED"}, {5, "LOITER"}, {6, "RTL"}, {7, "CIRCLE"},
            {9, "LAND"}, {11, "DRIFT"}, {13, "SPORT"}, {14, "FLIP"},
            {15, "AUTOTUNE"}, {16, "POSHOLD"}, {17, "BRAKE"}, {18, "THROW"},
            {20, "GUIDED_NOGPS"}, {21, "SMART_RTL"}, {23, "FOLLOW"},
            {24, "ZIGZAG"}, {27, "AUTO_RTL"},
        };
        return t;
    }();
    return table;
}

const ModeTable *tableFor(int autopilot)
{
    if (autopilot == arducopter().autopilot)
        return &arducopter();
    return nullptr;
}

QString modeName(int autopilot, int customMode)
{
    if (const ModeTable *t = tableFor(autopilot)) {
        if (auto name = t->nameOf(customMode))
            return *name;
    }
    return QStringLiteral("MODE %1").arg(customMode);
}

std::optional<int> modeId(int autopilot, const QString &name)
{
    if (const ModeTable *t = tableFor(autopilot))
        return t->idOf(name);
    return std::nullopt;
}

const std::vector<QuickMode> &quickModes()
{
    static const std::vector<QuickMode> modes = {
        {"LOITER", "LOITER", QStringLiteral("Giữ vị trí (GPS) — phi công có thể chỉnh")},
        {"STAB",   "STABILIZE", QStringLiteral("Stabilize — điều khiển tay có tự cân bằng")},
        {"ALTH",   "ALT_HOLD", QStringLiteral("Giữ độ cao — giữ cao độ, tay ngang")},
        {"LAND",   "LAND", QStringLiteral("Hạ cánh — hạ xuống và disarm khi chạm đất")},
    };
    return modes;
}

const std::vector<QuickMode> &extraModes()
{
    static const std::vector<QuickMode> modes = {
        {"RTL",    "RTL", QStringLiteral("Bay về điểm cất cánh")},
        {"GUIDED", "GUIDED", QStringLiteral("Guided — nhận mục tiêu vị trí/vận tốc")},
    };
    return modes;
}

} // namespace gcs::domain::flight_modes
