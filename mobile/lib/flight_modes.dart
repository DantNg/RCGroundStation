// Bảng chế độ bay ArduCopter + danh sách chế độ nhanh — port từ
// domain/flight_modes.cpp của bản Qt.

/// custom_mode → tên chế độ (ArduCopter).
const Map<int, String> arduCopterModes = {
  0: 'STABILIZE', 1: 'ACRO', 2: 'ALT_HOLD', 3: 'AUTO',
  4: 'GUIDED', 5: 'LOITER', 6: 'RTL', 7: 'CIRCLE',
  9: 'LAND', 11: 'DRIFT', 13: 'SPORT', 14: 'FLIP',
  15: 'AUTOTUNE', 16: 'POSHOLD', 17: 'BRAKE', 18: 'THROW',
  20: 'GUIDED_NOGPS', 21: 'SMART_RTL', 23: 'FOLLOW',
  24: 'ZIGZAG', 27: 'AUTO_RTL',
};

String modeName(int customMode) => arduCopterModes[customMode] ?? 'MODE $customMode';

int? modeId(String name) {
  final target = name.trim().toUpperCase();
  for (final e in arduCopterModes.entries) {
    if (e.value == target) return e.key;
  }
  return null;
}

class QuickMode {
  final String label;
  final String mode;
  final String desc;
  const QuickMode(this.label, this.mode, this.desc);
}

/// Chế độ nhanh hiển thị trên ModeRail (khớp bản Qt).
const List<QuickMode> quickModes = [
  QuickMode('LOITER', 'LOITER', 'Giữ vị trí (GPS) — phi công có thể chỉnh'),
  QuickMode('STAB', 'STABILIZE', 'Stabilize — điều khiển tay có tự cân bằng'),
  QuickMode('ALTH', 'ALT_HOLD', 'Giữ độ cao — giữ cao độ, tay ngang'),
  QuickMode('RTL', 'RTL', 'Bay về điểm cất cánh'),
  QuickMode('GUIDED', 'GUIDED', 'Guided — nhận mục tiêu vị trí/vận tốc'),
];
