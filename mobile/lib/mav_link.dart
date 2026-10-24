// Link MAVLink qua UDP — port thu gọn của MavlinkLink (Qt) sang Dart.
//
// Bind một socket UDP và học địa chỉ phương tiện từ gói đầu tiên (kiểu "udpin"
// của pymavlink), hoặc gửi thẳng tới host:port đã biết ("udpout"). Nhận khung qua
// MavlinkParser (dart_mavlink, dialect ardupilotmega) và phát ra onFrame. Phía
// gửi dựng khung MAVLink v2 rồi bắn tới peer. Mọi lệnh cấp cao (arm/mode/takeoff/
// flyTo/manualControl) đóng gói ở đây — không nơi nào khác chạm MAVLink.
import 'dart:async';
import 'dart:io';

import 'package:dart_mavlink/mavlink.dart';
import 'package:dart_mavlink/dialects/ardupilotmega.dart' as ap;

typedef FrameHandler = void Function(MavlinkFrame frame);

class MavLink {
  static const int gcsSystem = 255;
  static const int gcsComponent = 190; // MAV_COMP_ID_MISSIONPLANNER

  final _dialect = ap.MavlinkDialectArdupilotmega();
  MavlinkParser? _parser;
  RawDatagramSocket? _socket;
  StreamSubscription? _frameSub;
  Timer? _heartbeatTimer;

  InternetAddress? _peer;
  int _peerPort = 0;
  int _seq = 0;

  int targetSystem = 0;
  int targetComponent = 0;

  FrameHandler? onFrame;

  bool get isOpen => _socket != null;

  /// Mở UDP. [host] rỗng → nghe trên [port] (học peer từ gói đầu, hợp bộ mô
  /// phỏng dùng udpout). [host] có giá trị → gửi thẳng tới host:port.
  Future<void> connectUdp({String host = '', required int port}) async {
    await close();
    _parser = MavlinkParser(_dialect);
    _frameSub = _parser!.stream.listen((f) {
      _learnTarget(f);
      onFrame?.call(f);
    });

    if (host.isEmpty) {
      _socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, port);
    } else {
      _socket = await RawDatagramSocket.bind(InternetAddress.anyIPv4, 0);
      _peer = (await InternetAddress.lookup(host)).first;
      _peerPort = port;
    }
    _socket!.listen(_onSocketEvent);

    _heartbeatTimer =
        Timer.periodic(const Duration(seconds: 1), (_) => sendHeartbeat());
  }

  Future<void> close() async {
    _heartbeatTimer?.cancel();
    _heartbeatTimer = null;
    await _frameSub?.cancel();
    _frameSub = null;
    _socket?.close();
    _socket = null;
    _parser = null;
    _peer = null;
    _peerPort = 0;
    targetSystem = 0;
    targetComponent = 0;
  }

  void _onSocketEvent(RawSocketEvent event) {
    if (event != RawSocketEvent.read) return;
    final dg = _socket!.receive();
    if (dg == null) return;
    _peer ??= dg.address;
    if (_peerPort == 0) _peerPort = dg.port;
    _parser!.parse(dg.data);
  }

  void _learnTarget(MavlinkFrame f) {
    final m = f.message;
    if (m is ap.Heartbeat) {
      if (f.systemId == gcsSystem) return;
      if (m.autopilot == ap.mavAutopilotInvalid || m.type == ap.mavTypeGcs) {
        return;
      }
      targetSystem = f.systemId;
      targetComponent = f.componentId;
    }
  }

  void _send(MavlinkMessage msg) {
    final s = _socket;
    if (s == null || _peer == null || _peerPort == 0) return;
    final frame = MavlinkFrame.v2(_seq++ & 0xff, gcsSystem, gcsComponent, msg);
    s.send(frame.serialize(), _peer!, _peerPort);
  }

  // ── lệnh ──────────────────────────────────────────────────────────────────
  void _commandLong(int command,
      {double p1 = 0, double p2 = 0, double p3 = 0, double p4 = 0,
      double p5 = 0, double p6 = 0, double p7 = 0}) {
    if (targetSystem == 0) return;
    _send(ap.CommandLong(
      param1: p1, param2: p2, param3: p3, param4: p4,
      param5: p5, param6: p6, param7: p7,
      command: command,
      targetSystem: targetSystem, targetComponent: targetComponent,
      confirmation: 0,
    ));
  }

  void arm({bool force = false}) =>
      _commandLong(ap.mavCmdComponentArmDisarm, p1: 1, p2: force ? 21196 : 0);
  void disarm({bool force = false}) =>
      _commandLong(ap.mavCmdComponentArmDisarm, p1: 0, p2: force ? 21196 : 0);

  void setMode(int customMode) {
    if (targetSystem == 0) return;
    // SET_MODE cổ điển + đai an toàn DO_SET_MODE (như bản Qt).
    _send(ap.SetMode(
      customMode: customMode,
      targetSystem: targetSystem,
      baseMode: ap.mavModeFlagCustomModeEnabled,
    ));
    _commandLong(ap.mavCmdDoSetMode,
        p1: ap.mavModeFlagCustomModeEnabled.toDouble(),
        p2: customMode.toDouble());
  }

  void setModeByName(String name) {
    final id = modeIdFor(name);
    if (id != null) setMode(id);
  }

  void takeoff(double altitudeM) =>
      _commandLong(ap.mavCmdNavTakeoff, p7: altitudeM);

  void flyTo(double lat, double lon, double altRel) {
    if (targetSystem == 0) return;
    // type_mask: chỉ dùng vị trí (bật mọi bit vel/acc/yaw).
    const posOnly = 0x0DF8;
    _send(ap.SetPositionTargetGlobalInt(
      timeBootMs: 0,
      latInt: (lat * 1e7).round(),
      lonInt: (lon * 1e7).round(),
      alt: altRel,
      vx: 0, vy: 0, vz: 0, afx: 0, afy: 0, afz: 0, yaw: 0, yawRate: 0,
      typeMask: posOnly,
      targetSystem: targetSystem, targetComponent: targetComponent,
      coordinateFrame: ap.mavFrameGlobalRelativeAltInt,
    ));
  }

  /// Cần lái ảo. x=pitch, y=roll ∈[-1000,1000]; z=ga ∈[0,1000]; r=yaw ∈[-1000,1000].
  void manualControl(int x, int y, int z, int r, int buttons) {
    if (targetSystem == 0) return;
    _send(ap.ManualControl(
      x: x, y: y, z: z, r: r, buttons: buttons, target: targetSystem,
      buttons2: 0, enabledExtensions: 0,
      s: 0, t: 0, aux1: 0, aux2: 0, aux3: 0, aux4: 0, aux5: 0, aux6: 0,
    ));
  }

  void sendHeartbeat() {
    _send(ap.Heartbeat(
      customMode: 0,
      type: ap.mavTypeGcs,
      autopilot: ap.mavAutopilotInvalid,
      baseMode: 0,
      systemStatus: 0,
      mavlinkVersion: 3,
    ));
  }
}

// đặt ở đây để tránh vòng import với flight_modes; ánh xạ tên → id ArduCopter.
int? modeIdFor(String name) {
  const modes = {
    'STABILIZE': 0, 'ACRO': 1, 'ALT_HOLD': 2, 'AUTO': 3, 'GUIDED': 4,
    'LOITER': 5, 'RTL': 6, 'CIRCLE': 7, 'LAND': 9, 'DRIFT': 11, 'SPORT': 13,
    'FLIP': 14, 'AUTOTUNE': 15, 'POSHOLD': 16, 'BRAKE': 17, 'THROW': 18,
    'GUIDED_NOGPS': 20, 'SMART_RTL': 21, 'FOLLOW': 23, 'ZIGZAG': 24,
    'AUTO_RTL': 27,
  };
  return modes[name.trim().toUpperCase()];
}
