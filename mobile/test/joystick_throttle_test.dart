// Kiểm tra thuật toán THROTTLE tích phân tốc độ (mode 3, scale 0.4).
import 'package:flutter_test/flutter_test.dart';
import 'package:gcs_mobile/joystick_controller.dart';
import 'package:gcs_mobile/mav_link.dart';

void main() {
  const dt = 1 / JoystickController.rateHz;

  JoystickController make() {
    final j = JoystickController(MavLink())..enabled = true; // ga bắt đầu 0
    return j;
  }

  test('đẩy cần lên hết: ga tăng ~0.4/giây', () {
    final j = make();
    j.setAxes(0, 0, 0, 1.0); // full up
    for (var i = 0; i < JoystickController.rateHz; i++) {
      j.step(dt); // mô phỏng 1 giây
    }
    expect(j.throttleLevel.value, closeTo(0.4, 0.02));
    j.dispose();
  });

  test('nhả cần (giữa): ga GIỮ nguyên, không nhảy/rơi', () {
    final j = make();
    j.setAxes(0, 0, 0, 1.0);
    for (var i = 0; i < JoystickController.rateHz; i++) {
      j.step(dt);
    }
    final held = j.throttleLevel.value;
    j.setAxes(0, 0, 0, 0.0); // nhả tay → cần về giữa
    for (var i = 0; i < 50; i++) {
      j.step(dt);
    }
    expect(j.throttleLevel.value, closeTo(held, 1e-9));
    j.dispose();
  });

  test('đẩy cần xuống: ga giảm về 0 và kẹp không âm', () {
    final j = make();
    j.setAxes(0, 0, 0, 1.0);
    for (var i = 0; i < JoystickController.rateHz; i++) {
      j.step(dt); // lên ~0.4
    }
    j.setAxes(0, 0, 0, -1.0);
    for (var i = 0; i < 2 * JoystickController.rateHz; i++) {
      j.step(dt); // xuống > 0.4 → kẹp 0
    }
    expect(j.throttleLevel.value, 0.0);
    j.dispose();
  });

  test('deadband: lệch nhỏ không làm trôi ga', () {
    final j = make();
    j.setAxes(0, 0, 0, 0.03); // trong deadband
    for (var i = 0; i < 100; i++) {
      j.step(dt);
    }
    expect(j.throttleLevel.value, 0.0);
    j.dispose();
  });

  test('kẹp trên tại 1.0 khi giữ lên lâu', () {
    final j = make();
    j.setAxes(0, 0, 0, 1.0);
    for (var i = 0; i < 10 * JoystickController.rateHz; i++) {
      j.step(dt);
    }
    expect(j.throttleLevel.value, 1.0);
    j.dispose();
  });

  test('bật lại: ga reset về 0', () {
    final j = make();
    j.setAxes(0, 0, 0, 1.0);
    for (var i = 0; i < JoystickController.rateHz; i++) {
      j.step(dt);
    }
    j.enabled = false;
    j.enabled = true;
    expect(j.throttleLevel.value, 0.0);
    j.dispose();
  });
}
