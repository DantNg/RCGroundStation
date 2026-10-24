// Cần lái ảo — port JoystickController (Qt) sang Dart.
//
// Khi BẬT, bơm MANUAL_CONTROL tới phương tiện ở ~25 Hz. roll/pitch/yaw là TUYỆT
// ĐỐI (cần tự về giữa → 0). THROTTLE dùng thuật toán TÍCH PHÂN TỐC ĐỘ (mode 3),
// hợp với cần tự về giữa: trục dọc cần = *tốc độ thay đổi* ga, không phải ga tuyệt
// đối. Đẩy lên → ga tăng dần; đẩy xuống → giảm dần; nhả tay (cần về giữa) → ga
// GIỮ nguyên. Nhờ tích phân ở tick 25 Hz nên giữ cần yên vẫn tăng đều.
//   roll, pitch, yaw   ∈ [-1, 1] (0 = giữa)
//   throttleCmd (dọc)  ∈ [-1, 1] (lên = tăng ga, 0 = giữ)
import 'dart:async';

import 'package:flutter/foundation.dart';

import 'mav_link.dart';

class JoystickController extends ChangeNotifier {
  static const int rateHz = 25;
  /// Scale tích phân: đơn vị ga (0..1) mỗi giây khi đẩy cần hết cỡ.
  static const double throttleScale = 0.4;
  static const double _deadband = 0.05;

  final MavLink link;

  /// Mức ga hiện tại (0..1) — để UI hiện thanh ga (cần tự về giữa nên không
  /// suy ra được ga từ vị trí cần). Cập nhật mỗi tick.
  final ValueNotifier<double> throttleLevel = ValueNotifier<double>(0);

  Timer? _timer;
  bool _enabled = false;

  double _roll = 0, _pitch = 0, _yaw = 0;
  double _throttleCmd = 0; // lệnh tốc độ ga từ trục dọc cần (-1..1)
  double _throttle = 0; // ga tích luỹ (0..1)
  int _buttons = 0;

  JoystickController(this.link);

  bool get enabled => _enabled;

  set enabled(bool on) {
    if (_enabled == on) return;
    _enabled = on;
    if (on) {
      // luôn bắt đầu từ ga 0 khi bật (an toàn, ramp lên từ đáy)
      _throttle = 0;
      _roll = _pitch = _yaw = _throttleCmd = 0;
      throttleLevel.value = 0;
      _timer = Timer.periodic(
          const Duration(milliseconds: 1000 ~/ rateHz), (_) => step(1 / rateHz));
    } else {
      _timer?.cancel();
      _timer = null;
      _roll = _pitch = _yaw = _throttleCmd = 0;
      _buttons = 0;
      _send(); // khung trung tính cuối (cần về giữa)
    }
    notifyListeners();
  }

  /// roll/pitch/yaw tuyệt đối ∈[-1,1]; [throttleCmd] là TỐC ĐỘ ga ∈[-1,1].
  void setAxes(double roll, double pitch, double yaw, double throttleCmd) {
    _roll = roll;
    _pitch = pitch;
    _yaw = yaw;
    _throttleCmd = throttleCmd;
  }

  void setButtons(int mask) => _buttons = mask;

  /// Một bước tích phân + gửi khung. [dt] giây. Tách ra để test được.
  @visibleForTesting
  void step(double dt) {
    final cmd = _throttleCmd.abs() > _deadband ? _throttleCmd : 0.0;
    _throttle = (_throttle + cmd * throttleScale * dt).clamp(0.0, 1.0);
    throttleLevel.value = _throttle;
    _send();
  }

  int _axis(double v) => (v.clamp(-1.0, 1.0) * 1000).round();
  int _throttleMav(double v) => (v.clamp(0.0, 1.0) * 1000).round();

  void _send() {
    // x=pitch, y=roll, z=throttle, r=yaw (thang MAVLink). link tự bỏ nếu chưa
    // có mục tiêu.
    link.manualControl(
        _axis(_pitch), _axis(_roll), _throttleMav(_throttle), _axis(_yaw), _buttons);
  }

  @override
  void dispose() {
    _timer?.cancel();
    throttleLevel.dispose();
    super.dispose();
  }
}
