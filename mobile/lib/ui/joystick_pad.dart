// Cần lái ảo tái sử dụng — đế tròn + núm kéo, tự về giữa khi nhả. Port JoystickPad.qml.
// onChanged(nx, ny): nx dương = phải, ny dương = LÊN. Nhả → (0,0).
import 'package:flutter/material.dart';

import '../theme.dart';

class JoystickPad extends StatefulWidget {
  final double size;
  final String label;
  final String axisH;
  final String axisV;
  final void Function(double nx, double ny) onChanged;

  const JoystickPad({
    super.key,
    this.size = 132,
    required this.label,
    required this.axisH,
    required this.axisV,
    required this.onChanged,
  });

  @override
  State<JoystickPad> createState() => _JoystickPadState();
}

class _JoystickPadState extends State<JoystickPad> {
  Offset _knob = Offset.zero; // px lệch từ tâm
  bool _pressed = false;

  double get _base => widget.size / 2;
  double get _knobR => _base * 0.34;
  double get _maxTravel => _base - _knobR;

  void _update(Offset local) {
    var d = local - Offset(_base, _base);
    final dist = d.distance;
    if (dist > _maxTravel && dist > 0) d = d * (_maxTravel / dist);
    setState(() => _knob = d);
    widget.onChanged(d.dx / _maxTravel, -d.dy / _maxTravel);
  }

  void _release() {
    setState(() {
      _knob = Offset.zero;
      _pressed = false;
    });
    widget.onChanged(0, 0);
  }

  @override
  Widget build(BuildContext context) {
    return GestureDetector(
      onPanStart: (d) {
        setState(() => _pressed = true);
        _update(d.localPosition);
      },
      onPanUpdate: (d) => _update(d.localPosition),
      onPanEnd: (_) => _release(),
      onPanCancel: _release,
      child: SizedBox(
        width: widget.size,
        height: widget.size,
        child: CustomPaint(
          painter: _PadPainter(
            knob: _knob,
            knobR: _knobR,
            pressed: _pressed,
            axisH: widget.axisH,
            axisV: widget.axisV,
            label: widget.label,
          ),
        ),
      ),
    );
  }
}

class _PadPainter extends CustomPainter {
  final Offset knob;
  final double knobR;
  final bool pressed;
  final String axisH, axisV, label;

  _PadPainter({
    required this.knob,
    required this.knobR,
    required this.pressed,
    required this.axisH,
    required this.axisV,
    required this.label,
  });

  @override
  void paint(Canvas canvas, Size size) {
    final center = Offset(size.width / 2, size.height / 2);
    final baseR = size.width / 2;

    // đế
    canvas.drawCircle(center, baseR - 1, Paint()..color = const Color(0xCC0A0D0B));
    canvas.drawCircle(
        center,
        baseR - 1,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 1.5
          ..color = pressed ? AppTheme.accent : AppTheme.strokeBtn);

    // vạch thập
    final cross = Paint()
      ..color = AppTheme.strokeSoft
      ..strokeWidth = 1;
    canvas.drawLine(Offset(center.dx - baseR * 0.7, center.dy),
        Offset(center.dx + baseR * 0.7, center.dy), cross);
    canvas.drawLine(Offset(center.dx, center.dy - baseR * 0.7),
        Offset(center.dx, center.dy + baseR * 0.7), cross);

    // nhãn trục
    _text(canvas, axisV, Offset(center.dx, 10), 8, AppTheme.dim,
        align: TextAlign.center);
    _text(canvas, axisH, Offset(size.width - 4, center.dy), 8, AppTheme.dim,
        align: TextAlign.right, anchorRight: true);

    // núm
    final kc = center + knob;
    canvas.drawCircle(kc, knobR,
        Paint()..color = pressed ? AppTheme.accent : const Color(0xD937E0A0));
    canvas.drawCircle(
        kc,
        knobR,
        Paint()
          ..style = PaintingStyle.stroke
          ..strokeWidth = 2
          ..color = const Color(0xFF0A0D0B));

    // nhãn dưới
    _text(canvas, label, Offset(center.dx, size.height - 12), 9, AppTheme.sub,
        align: TextAlign.center);
  }

  void _text(Canvas canvas, String s, Offset at, double size, Color color,
      {TextAlign align = TextAlign.left, bool anchorRight = false}) {
    if (s.isEmpty) return;
    final tp = TextPainter(
      text: TextSpan(
          text: s,
          style: TextStyle(
              fontFamily: AppTheme.mono, fontSize: size, color: color)),
      textDirection: TextDirection.ltr,
      textAlign: align,
    )..layout();
    var dx = at.dx;
    if (align == TextAlign.center) dx -= tp.width / 2;
    if (anchorRight) dx = at.dx - tp.width;
    tp.paint(canvas, Offset(dx, at.dy - tp.height / 2));
  }

  @override
  bool shouldRepaint(_PadPainter old) =>
      old.knob != knob || old.pressed != pressed;
}
