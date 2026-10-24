// Kiểm tra lọc nguồn + la bàn ổn định trong GcsController — tái hiện đúng lỗi
// "mode/ARM/heading nhảy liên tục" và xác nhận đã sửa.
import 'package:dart_mavlink/mavlink.dart';
import 'package:dart_mavlink/dialects/ardupilotmega.dart' as ap;
import 'package:flutter_test/flutter_test.dart';
import 'package:gcs_mobile/controller.dart';

MavlinkFrame _hb(int sys, int comp,
    {required int autopilot,
    required int type,
    required int baseMode,
    required int customMode}) {
  return MavlinkFrame.v2(
    0,
    sys,
    comp,
    ap.Heartbeat(
      customMode: customMode,
      type: type,
      autopilot: autopilot,
      baseMode: baseMode,
      systemStatus: 4,
      mavlinkVersion: 3,
    ),
  );
}

MavlinkFrame _att(int sys, double yawRad) => MavlinkFrame.v2(
    0,
    sys,
    1,
    ap.Attitude(
        timeBootMs: 0,
        roll: 0,
        pitch: 0,
        yaw: yawRad,
        rollspeed: 0,
        pitchspeed: 0,
        yawspeed: 0));

MavlinkFrame _pos(int sys, int hdgCentideg) => MavlinkFrame.v2(
    0,
    sys,
    1,
    ap.GlobalPositionInt(
        timeBootMs: 0,
        lat: 210285000,
        lon: 1058048000,
        alt: 10000,
        relativeAlt: 5000,
        vx: 0,
        vy: 0,
        vz: 0,
        hdg: hdgCentideg));

void main() {
  const int arduPilot = 3; // MAV_AUTOPILOT_ARDUPILOTMEGA
  const int quad = 2; // MAV_TYPE_QUADROTOR
  const int armed = 128; // MAV_MODE_FLAG_SAFETY_ARMED

  test('mode/ARM chỉ theo flight controller đã khoá, không nhảy', () {
    final c = GcsController();
    c.link.targetSystem = 1; // đã khoá phương tiện 1

    // heartbeat autopilot thật: ARMED, mode LOITER(5)
    c.link.onFrame!(_hb(1, 1,
        autopilot: arduPilot, type: quad, baseMode: armed, customMode: 5));
    expect(c.telemetry.armed, isTrue);
    expect(c.telemetry.customMode, 5);

    // heartbeat companion (autopilot INVALID): disarmed, mode 0 → phải BỎ QUA
    c.link.onFrame!(_hb(1, 191,
        autopilot: ap.mavAutopilotInvalid,
        type: 18,
        baseMode: 0,
        customMode: 0));
    expect(c.telemetry.armed, isTrue, reason: 'companion không được đổi ARM');
    expect(c.telemetry.customMode, 5, reason: 'companion không được đổi mode');

    // heartbeat hệ khác (system 2) → phải BỎ QUA
    c.link.onFrame!(_hb(2, 1,
        autopilot: arduPilot, type: quad, baseMode: 0, customMode: 0));
    expect(c.telemetry.armed, isTrue);
    expect(c.telemetry.customMode, 5);

    c.dispose();
  });

  test('heading không nhảy 0↔yaw khi hdg=0 lúc đứng yên', () {
    final c = GcsController();
    c.link.targetSystem = 1;

    // yaw 45° → heading 45
    c.link.onFrame!(_att(1, 45 * 3.141592653589793 / 180));
    expect(c.telemetry.heading, closeTo(45, 0.5));

    // GLOBAL_POSITION hdg=0 (đứng yên) → GIỮ 45, không về 0
    c.link.onFrame!(_pos(1, 0));
    expect(c.telemetry.heading, closeTo(45, 0.5),
        reason: 'hdg=0 không được ép heading về 0');

    // hdg hợp lệ 90° → theo định vị
    c.link.onFrame!(_pos(1, 9000));
    expect(c.telemetry.heading, closeTo(90, 0.5));

    // lại hdg=0 → GIỮ 90 (dùng heading định vị gần nhất)
    c.link.onFrame!(_pos(1, 0));
    expect(c.telemetry.heading, closeTo(90, 0.5));

    c.dispose();
  });
}
