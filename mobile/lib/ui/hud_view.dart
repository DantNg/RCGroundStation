// HUD — chân trời nhân tạo (roll/pitch) + vạch tâm. Port thu gọn HudView.qml.
import 'dart:math' as math;
import 'package:flutter/material.dart';

import '../telemetry.dart';
import '../theme.dart';

class HudView extends StatelessWidget {
  final Telemetry telemetry;
  const HudView({super.key, required this.telemetry});

  @override
  Widget build(BuildContext context) {
    return ClipRect(
      child: CustomPaint(
        painter: _HudPainter(telemetry),
        child: const SizedBox.expand(),
      ),
    );
  }
}

class _HudPainter extends CustomPainter {
  final Telemetry t;
  _HudPainter(this.t);

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    canvas.save();
    canvas.translate(center.dx, center.dy);
    canvas.rotate(-t.roll * math.pi / 180);
    // dịch theo pitch (px/độ)
    final pitchPx = t.pitch * 4.0;
    canvas.translate(0, pitchPx);

    final big = size.width > size.height ? size.width : size.height;
    final w = big * 3, h = big * 3;
    // trời
    canvas.drawRect(Rect.fromLTWH(-w / 2, -h / 2, w, h / 2),
        Paint()..color = AppTheme.sky);
    // đất
    canvas.drawRect(
        Rect.fromLTWH(-w / 2, 0, w, h / 2), Paint()..color = AppTheme.ground);
    // đường chân trời
    canvas.drawLine(Offset(-w / 2, 0), Offset(w / 2, 0),
        Paint()..color = Colors.white70..strokeWidth = 2);

    // vạch pitch
    final tick = Paint()..color = Colors.white54..strokeWidth = 1.5;
    for (int deg = -30; deg <= 30; deg += 10) {
      if (deg == 0) continue;
      final y = -deg * 4.0;
      canvas.drawLine(Offset(-40, y), Offset(40, y), tick);
    }
    canvas.restore();

    // vạch tâm (cố định)
    final marker = Paint()
      ..color = AppTheme.danger
      ..strokeWidth = 3;
    canvas.drawLine(Offset(center.dx - 40, center.dy),
        Offset(center.dx - 12, center.dy), marker);
    canvas.drawLine(Offset(center.dx + 12, center.dy),
        Offset(center.dx + 40, center.dy), marker);
    canvas.drawCircle(center, 3, marker);

    // đọc số
    _text(canvas, 'ROLL ${t.roll.toStringAsFixed(0)}°',
        Offset(12, size.height - 44));
    _text(canvas, 'PITCH ${t.pitch.toStringAsFixed(0)}°',
        Offset(12, size.height - 26));
    _text(canvas, 'HDG ${t.heading.toStringAsFixed(0)}°',
        Offset(size.width - 90, size.height - 26));
  }

  void _text(Canvas canvas, String s, Offset at) {
    final tp = TextPainter(
      text: TextSpan(
          text: s,
          style: const TextStyle(
              fontFamily: AppTheme.mono, fontSize: 12, color: AppTheme.text)),
      textDirection: TextDirection.ltr,
    )..layout();
    tp.paint(canvas, at);
  }

  @override
  bool shouldRepaint(_HudPainter old) =>
      old.t.roll != t.roll ||
      old.t.pitch != t.pitch ||
      old.t.heading != t.heading;
}
