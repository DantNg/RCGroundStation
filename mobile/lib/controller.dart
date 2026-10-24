// Gốc lắp ráp — port GcsController (Qt) sang Dart/Flutter.
//
// Sở hữu MavLink, cập nhật ảnh chụp Telemetry từ các khung đến, giữ hàng đợi
// thông báo (STATUSTEXT), và lộ lệnh cấp cao cho UI. Là ChangeNotifier: mọi khung
// cập nhật gọi notifyListeners() để widget vẽ lại (dirty-check nhẹ ở UI).
import 'dart:math' as math;

import 'package:flutter/foundation.dart';
import 'package:dart_mavlink/mavlink.dart';
import 'package:dart_mavlink/dialects/ardupilotmega.dart' as ap;

import 'mav_link.dart';
import 'telemetry.dart';

class GcsController extends ChangeNotifier {
  final MavLink link = MavLink();
  final Telemetry telemetry = Telemetry();
  final List<Notice> notices = [];

  double? _homeLat;
  double? _homeLon;

  // La bàn ổn định: giữ riêng heading từ yaw (Attitude) và từ định vị
  // (GLOBAL_POSITION.hdg / VFR_HUD.heading). Dùng heading định vị khi CÓ, ngược
  // lại lùi về yaw. Tránh nhấp nháy 0↔yaw khi hdg=0 lúc đứng yên (bẫy như bản Qt).
  double _yawHeading = 0;
  double? _fixHeading;

  GcsController() {
    link.onFrame = _onFrame;
  }

  bool get connected => telemetry.connected;

  Future<void> connectUdp({String host = '', int port = 14550}) async {
    await link.connectUdp(host: host, port: port);
    telemetry.connected = true;
    notifyListeners();
  }

  Future<void> disconnect() async {
    await link.close();
    telemetry.connected = false;
    telemetry.armed = false;
    _fixHeading = null;
    _yawHeading = 0;
    _homeLat = null;
    _homeLon = null;
    notifyListeners();
  }

  // ── lệnh (uỷ quyền cho link) ────────────────────────────────────────────
  void arm({bool force = false}) => link.arm(force: force);
  void disarm({bool force = false}) => link.disarm(force: force);
  void setModeByName(String name) => link.setModeByName(name);
  void takeoff(double altM) => link.takeoff(altM);
  void flyTo(double lat, double lon, double altRel) => link.flyTo(lat, lon, altRel);

  void _pushNotice(int severity, String text) {
    notices.add(Notice(severity, text));
    while (notices.length > 40) {
      notices.removeAt(0);
    }
  }

  void _onFrame(MavlinkFrame f) {
    final t = telemetry;
    final m = f.message;

    // ── LỌC NGUỒN ─────────────────────────────────────────────────────────
    // Bỏ heartbeat/telemetry của chính GCS (hoặc GCS khác trên mạng), và mọi
    // khung KHÔNG thuộc phương tiện đã khoá. Nếu không lọc, mode/ARM/heading của
    // nhiều nguồn (companion, gimbal, GCS khác) ghi đè lẫn nhau → nhấp nháy.
    if (f.systemId == MavLink.gcsSystem) return;
    if (link.targetSystem != 0 && f.systemId != link.targetSystem) return;

    if (m is ap.Heartbeat) {
      // Chỉ tin heartbeat của bộ điều khiển bay thật (autopilot hợp lệ, không
      // phải GCS). Gimbal/companion cũng phát heartbeat nhưng mode/armed vô
      // nghĩa — nhận nhầm sẽ làm trạng thái nhảy.
      if (m.autopilot == ap.mavAutopilotInvalid || m.type == ap.mavTypeGcs) {
        return;
      }
      t.autopilot = m.autopilot;
      t.customMode = m.customMode;
      t.systemStatus = m.systemStatus;
      t.armed = (m.baseMode & ap.mavModeFlagSafetyArmed) != 0;
    } else if (m is ap.Attitude) {
      t.roll = m.roll * 180 / math.pi;
      t.pitch = m.pitch * 180 / math.pi;
      t.yaw = m.yaw * 180 / math.pi;
      _yawHeading = (t.yaw + 360) % 360;
      t.heading = _fixHeading ?? _yawHeading;
    } else if (m is ap.GlobalPositionInt) {
      t.lat = m.lat / 1e7;
      t.lon = m.lon / 1e7;
      t.altRel = m.relativeAlt / 1000.0;
      // hdg=0 (đứng yên) hoặc 65535 (không rõ) = không có heading định vị.
      if (m.hdg != 0 && m.hdg != 65535) _fixHeading = m.hdg / 100.0;
      t.heading = _fixHeading ?? _yawHeading;
      _updateHome(t);
    } else if (m is ap.VfrHud) {
      t.groundSpeed = m.groundspeed;
      t.climb = m.climb;
      if (m.heading != 0) _fixHeading = m.heading.toDouble();
      t.heading = _fixHeading ?? _yawHeading;
    } else if (m is ap.SysStatus) {
      t.voltage = m.voltageBattery / 1000.0;
      t.current = m.currentBattery < 0 ? 0 : m.currentBattery / 100.0;
      t.batteryRemaining = m.batteryRemaining;
    } else if (m is ap.GpsRawInt) {
      t.gpsFix = m.fixType;
      t.satellites = m.satellitesVisible;
    } else if (m is ap.Statustext) {
      final text = String.fromCharCodes(
              m.text.where((c) => c != 0).map((c) => c & 0xff))
          .trim();
      if (text.isNotEmpty) _pushNotice(m.severity, text);
    }
    notifyListeners();
  }

  void _updateHome(Telemetry t) {
    if (t.gpsFix < 2) return;
    if (t.armed) {
      _homeLat ??= t.lat;
      _homeLon ??= t.lon;
    } else {
      // trước khi arm, home bám theo vị trí hiện tại
      _homeLat = t.lat;
      _homeLon = t.lon;
    }
    if (_homeLat != null && _homeLon != null) {
      t.homeDistance = _haversine(_homeLat!, _homeLon!, t.lat, t.lon);
    }
  }

  static double _haversine(double lat1, double lon1, double lat2, double lon2) {
    const r = 6371000.0;
    final dLat = (lat2 - lat1) * math.pi / 180;
    final dLon = (lon2 - lon1) * math.pi / 180;
    final a = math.sin(dLat / 2) * math.sin(dLat / 2) +
        math.cos(lat1 * math.pi / 180) *
            math.cos(lat2 * math.pi / 180) *
            math.sin(dLon / 2) *
            math.sin(dLon / 2);
    return r * 2 * math.atan2(math.sqrt(a), math.sqrt(1 - a));
  }

  @override
  void dispose() {
    link.close();
    super.dispose();
  }
}
