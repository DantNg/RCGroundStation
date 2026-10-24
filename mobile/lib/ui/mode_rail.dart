// Thanh chế độ bay (phải) — chế độ nhanh + CẤT/HẠ CÁNH + công tắc CẦN LÁI.
// Port ModeRail.qml.
import 'package:flutter/material.dart';

import '../controller.dart';
import '../flight_modes.dart';
import '../joystick_controller.dart';
import '../telemetry.dart';
import '../theme.dart';

class ModeRail extends StatelessWidget {
  final GcsController controller;
  final JoystickController joystick;
  const ModeRail({super.key, required this.controller, required this.joystick});

  @override
  Widget build(BuildContext context) {
    final t = controller.telemetry;
    return Container(
      color: AppTheme.panelSolid,
      padding: const EdgeInsets.all(8),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Padding(
            padding: EdgeInsets.only(bottom: 6),
            child: Text('CHẾ ĐỘ BAY',
                style: TextStyle(
                    fontFamily: AppTheme.ui,
                    color: AppTheme.dim,
                    fontSize: 9,
                    fontWeight: FontWeight.bold,
                    letterSpacing: 1.6)),
          ),
          ...quickModes.map((qm) => Expanded(child: _modeBtn(t, qm))),
          const SizedBox(height: 6),
          const Divider(height: 1, color: AppTheme.strokeSoft),
          const SizedBox(height: 6),
          _actionBtn('▲', 'CẤT CÁNH', 'chạm để chỉnh', AppTheme.accent, t.connected,
              () => _takeoffDialog(context)),
          const SizedBox(height: 6),
          _actionBtn('▼', 'HẠ CÁNH', 'hạ cánh & tắt động cơ', AppTheme.warn,
              t.connected, () => controller.setModeByName('LAND')),
          const SizedBox(height: 6),
          _joystickToggle(t.connected),
        ],
      ),
    );
  }

  Widget _modeBtn(Telemetry t, QuickMode qm) {
    final active = t.connected && t.modeLabel == qm.mode;
    return Padding(
      padding: const EdgeInsets.only(bottom: 6),
      child: Opacity(
        opacity: t.connected ? 1 : 0.45,
        child: GestureDetector(
          onTap: t.connected ? () => controller.setModeByName(qm.mode) : null,
          child: Container(
            decoration: BoxDecoration(
              color: active ? const Color(0x22FF3B3B) : AppTheme.panel,
              border: Border.all(
                  color: active ? AppTheme.danger : AppTheme.strokeBtn),
              borderRadius: BorderRadius.circular(6),
            ),
            padding: const EdgeInsets.symmetric(horizontal: 10, vertical: 4),
            child: Column(
              mainAxisAlignment: MainAxisAlignment.center,
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(qm.label,
                    style: TextStyle(
                        fontFamily: AppTheme.mono,
                        color: active
                            ? const Color(0xFFFFE9E9)
                            : const Color(0xFFB9B7C2),
                        fontSize: 12,
                        fontWeight: FontWeight.bold)),
                Text(qm.desc,
                    maxLines: 1,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                        fontFamily: AppTheme.ui,
                        color: active ? const Color(0xFFE9C9C9) : AppTheme.dim,
                        fontSize: 8)),
              ],
            ),
          ),
        ),
      ),
    );
  }

  Widget _actionBtn(String glyph, String label, String sub, Color tint,
      bool enabled, VoidCallback onTap) {
    return Opacity(
      opacity: enabled ? 1 : 0.4,
      child: GestureDetector(
        onTap: enabled ? onTap : null,
        child: Container(
          height: 40,
          decoration: BoxDecoration(
            color: tint.withValues(alpha: 0.14),
            border: Border.all(color: tint),
            borderRadius: BorderRadius.circular(6),
          ),
          padding: const EdgeInsets.symmetric(horizontal: 9),
          child: Row(
            children: [
              Text(glyph,
                  style: TextStyle(
                      color: tint, fontSize: 15, fontWeight: FontWeight.bold)),
              const SizedBox(width: 7),
              Column(
                mainAxisAlignment: MainAxisAlignment.center,
                crossAxisAlignment: CrossAxisAlignment.start,
                children: [
                  Text(label,
                      style: TextStyle(
                          fontFamily: AppTheme.mono,
                          color: tint,
                          fontSize: 13,
                          fontWeight: FontWeight.bold)),
                  Text(sub,
                      style: const TextStyle(
                          fontFamily: AppTheme.ui,
                          color: AppTheme.dim,
                          fontSize: 8)),
                ],
              ),
            ],
          ),
        ),
      ),
    );
  }

  Widget _joystickToggle(bool enabled) {
    return ListenableBuilder(
      listenable: joystick,
      builder: (context, _) {
        final on = joystick.enabled;
        return Opacity(
          opacity: enabled ? 1 : 0.4,
          child: GestureDetector(
            onTap: enabled ? () => joystick.enabled = !on : null,
            child: Container(
              height: 40,
              decoration: BoxDecoration(
                color: on ? AppTheme.accent.withValues(alpha: 0.20) : AppTheme.panel,
                border:
                    Border.all(color: on ? AppTheme.accent : AppTheme.strokeBtn),
                borderRadius: BorderRadius.circular(6),
              ),
              padding: const EdgeInsets.symmetric(horizontal: 9),
              child: Row(
                children: [
                  Icon(Icons.gps_fixed,
                      color: on ? AppTheme.accent : AppTheme.sub, size: 16),
                  const SizedBox(width: 7),
                  Expanded(
                    child: Column(
                      mainAxisAlignment: MainAxisAlignment.center,
                      crossAxisAlignment: CrossAxisAlignment.start,
                      children: [
                        Text('CẦN LÁI',
                            style: TextStyle(
                                fontFamily: AppTheme.mono,
                                color: on ? AppTheme.accent : AppTheme.sub,
                                fontSize: 13,
                                fontWeight: FontWeight.bold)),
                        Text(on ? 'đang bật · chạm để tắt' : 'chạm để bật cần lái',
                            maxLines: 1,
                            overflow: TextOverflow.ellipsis,
                            style: const TextStyle(
                                fontFamily: AppTheme.ui,
                                color: AppTheme.dim,
                                fontSize: 8)),
                      ],
                    ),
                  ),
                  Container(
                    width: 9,
                    height: 9,
                    decoration: BoxDecoration(
                      shape: BoxShape.circle,
                      color: on ? AppTheme.accent : AppTheme.strokeBtn,
                    ),
                  ),
                ],
              ),
            ),
          ),
        );
      },
    );
  }

  void _takeoffDialog(BuildContext context) {
    int alt = 5;
    showDialog(
      context: context,
      builder: (ctx) => StatefulBuilder(
        builder: (ctx, setSt) => AlertDialog(
          backgroundColor: const Color(0xF20A0D0B),
          shape: RoundedRectangleBorder(
            borderRadius: BorderRadius.circular(12),
            side: const BorderSide(color: AppTheme.accent),
          ),
          title: const Text('▲ CẤT CÁNH',
              style: TextStyle(
                  fontFamily: AppTheme.mono,
                  color: AppTheme.accent,
                  fontWeight: FontWeight.bold)),
          content: Row(
            mainAxisAlignment: MainAxisAlignment.center,
            children: [
              _stepBtn('−', () => setSt(() => alt = (alt - 1).clamp(1, 500))),
              const SizedBox(width: 12),
              Text('$alt m',
                  style: const TextStyle(
                      fontFamily: AppTheme.mono,
                      color: AppTheme.text,
                      fontSize: 24,
                      fontWeight: FontWeight.bold)),
              const SizedBox(width: 12),
              _stepBtn('+', () => setSt(() => alt = (alt + 1).clamp(1, 500))),
            ],
          ),
          actions: [
            TextButton(
                onPressed: () => Navigator.pop(ctx),
                child: const Text('HUỶ',
                    style: TextStyle(color: AppTheme.sub))),
            TextButton(
                onPressed: () {
                  controller.takeoff(alt.toDouble());
                  Navigator.pop(ctx);
                },
                child: const Text('CẤT CÁNH',
                    style: TextStyle(
                        color: AppTheme.accent, fontWeight: FontWeight.bold))),
          ],
        ),
      ),
    );
  }

  Widget _stepBtn(String s, VoidCallback onTap) => GestureDetector(
        onTap: onTap,
        child: Container(
          width: 46,
          height: 46,
          decoration: BoxDecoration(
            color: AppTheme.panel,
            border: Border.all(color: AppTheme.strokeBtn),
            borderRadius: BorderRadius.circular(8),
          ),
          alignment: Alignment.center,
          child: Text(s,
              style: const TextStyle(
                  fontFamily: AppTheme.mono,
                  color: AppTheme.accent,
                  fontSize: 24,
                  fontWeight: FontWeight.bold)),
        ),
      );
}
