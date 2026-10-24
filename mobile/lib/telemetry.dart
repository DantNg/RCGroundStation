// Ảnh chụp telemetry — port TelemetrySnapshot của bản Qt (thu gọn cho mobile).
import 'flight_modes.dart';

class Telemetry {
  bool connected = false;
  bool armed = false;
  int autopilot = 0;
  int customMode = 0;
  int systemStatus = 0;

  // tư thế (độ)
  double roll = 0;
  double pitch = 0;
  double yaw = 0;

  // vị trí
  double lat = 0;
  double lon = 0;
  double altRel = 0; // độ cao tương đối (m)
  double heading = 0; // độ

  // vận tốc
  double groundSpeed = 0; // m/s
  double climb = 0;

  // pin
  double voltage = 0; // V
  double current = 0; // A
  int batteryRemaining = -1; // %

  // GPS
  int gpsFix = 0;
  int satellites = 0;

  double homeDistance = 0; // m

  bool get hasFix => gpsFix >= 2;
  String get modeLabel => modeName(customMode);

  Telemetry clone() {
    final t = Telemetry();
    t.connected = connected;
    t.armed = armed;
    t.autopilot = autopilot;
    t.customMode = customMode;
    t.systemStatus = systemStatus;
    t.roll = roll;
    t.pitch = pitch;
    t.yaw = yaw;
    t.lat = lat;
    t.lon = lon;
    t.altRel = altRel;
    t.heading = heading;
    t.groundSpeed = groundSpeed;
    t.climb = climb;
    t.voltage = voltage;
    t.current = current;
    t.batteryRemaining = batteryRemaining;
    t.gpsFix = gpsFix;
    t.satellites = satellites;
    t.homeDistance = homeDistance;
    return t;
  }
}

/// Mức nghiêm trọng thông báo (MAV_SEVERITY): nhỏ = nặng hơn.
class Notice {
  final int severity;
  final String text;
  final DateTime time;
  Notice(this.severity, this.text) : time = DateTime.now();
  bool get isCritical => severity <= 3;
  bool get isWarning => severity == 4;
}
