// Lớp phủ cần lái ảo — hai cần ở hai góc dưới, chỉ hiện khi joystick.enabled.
// Trái: ga (dọc) / yaw (ngang). Phải: pitch (dọc) / roll (ngang). Port JoystickOverlay.qml.
import 'package:flutter/material.dart';

import '../joystick_controller.dart';
import '../theme.dart';
import 'joystick_pad.dart';

class JoystickOverlay extends StatefulWidget {
  final JoystickController joystick;
  const JoystickOverlay({super.key, required this.joystick});

  @override
  State<JoystickOverlay> createState() => _JoystickOverlayState();
}

class _JoystickOverlayState extends State<JoystickOverlay> {
  double _roll = 0, _pitch = 0, _yaw = 0, _throttleCmd = 0;

  void _push() =>
      widget.joystick.setAxes(_roll, _pitch, _yaw, _throttleCmd);

  @override
  Widget build(BuildContext context) {
    return ListenableBuilder(
      listenable: widget.joystick,
      builder: (context, _) {
        if (!widget.joystick.enabled) return const SizedBox.shrink();
        return Stack(
          children: [
            // banner cảnh báo an toàn
            Positioned(
              top: 8,
              left: 0,
              right: 0,
              child: Center(child: _warnBanner()),
            ),
            // cần trái: ga (tích phân) + yaw, kèm thanh mức ga
            Positioned(
              left: 14,
              bottom: 14,
              child: Row(
                crossAxisAlignment: CrossAxisAlignment.end,
                children: [
                  JoystickPad(
                    label: 'GA↕ · YAW↔',
                    axisV: 'GA+/-',
                    axisH: 'YAW',
                    onChanged: (nx, ny) {
                      _yaw = nx;
                      _throttleCmd = ny; // trục dọc = TỐC ĐỘ ga (tích phân)
                      _push();
                    },
                  ),
                  const SizedBox(width: 8),
                  _throttleBar(),
                ],
              ),
            ),
            // cần phải: pitch + roll
            Positioned(
              right: 14,
              bottom: 14,
              child: JoystickPad(
                label: 'PITCH · ROLL',
                axisV: 'PITCH',
                axisH: 'ROLL',
                onChanged: (nx, ny) {
                  _roll = nx;
                  _pitch = ny;
                  _push();
                },
              ),
            ),
          ],
        );
      },
    );
  }

  // thanh mức ga (tích luỹ) — cần thiết vì cần tự về giữa, không suy ga từ vị trí.
  Widget _throttleBar() => ValueListenableBuilder<double>(
        valueListenable: widget.joystick.throttleLevel,
        builder: (context, level, _) {
          return SizedBox(
            width: 30,
            height: 132,
            child: Column(
              children: [
                Text('${(level * 100).round()}%',
                    style: const TextStyle(
                        fontFamily: AppTheme.mono,
                        color: AppTheme.accent,
                        fontSize: 10,
                        fontWeight: FontWeight.bold)),
                const SizedBox(height: 2),
                Expanded(
                  child: Container(
                    width: 18,
                    decoration: BoxDecoration(
                      color: const Color(0xCC0A0D0B),
                      border: Border.all(color: AppTheme.strokeBtn),
                      borderRadius: BorderRadius.circular(4),
                    ),
                    child: Align(
                      alignment: Alignment.bottomCenter,
                      child: FractionallySizedBox(
                        heightFactor: level.clamp(0.0, 1.0),
                        widthFactor: 1,
                        child: Container(
                          decoration: BoxDecoration(
                            color: AppTheme.accent,
                            borderRadius: BorderRadius.circular(3),
                          ),
                        ),
                      ),
                    ),
                  ),
                ),
                const Text('GA',
                    style: TextStyle(
                        fontFamily: AppTheme.mono,
                        color: AppTheme.dim,
                        fontSize: 8)),
              ],
            ),
          );
        },
      );

  Widget _warnBanner() => Container(
        padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 5),
        decoration: BoxDecoration(
          color: const Color(0xCC1A0F0F),
          border: Border.all(color: AppTheme.warn),
          borderRadius: BorderRadius.circular(13),
        ),
        child: const Row(
          mainAxisSize: MainAxisSize.min,
          children: [
            Icon(Icons.circle, color: AppTheme.warn, size: 8),
            SizedBox(width: 8),
            Text(
              'CẦN LÁI ẢO ĐANG BẬT — cần chế độ tay (STABILIZE/ALT_HOLD) & đã ARM',
              style: TextStyle(
                  fontFamily: AppTheme.mono, color: AppTheme.warn, fontSize: 9),
            ),
          ],
        ),
      );
}
