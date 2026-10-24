// Khung "GroundController Handheld" — status bar / [bản đồ|HUD + rail] / dải
// telemetry, phủ cần lái ảo + thẻ kết nối. Port từ Main.qml.
import 'package:flutter/material.dart';

import '../controller.dart';
import '../joystick_controller.dart';
import '../telemetry.dart';
import '../theme.dart';
import 'connection_card.dart';
import 'hud_view.dart';
import 'joystick_overlay.dart';
import 'map_view.dart';
import 'mode_rail.dart';
import 'warning_banners.dart';

class HandheldScreen extends StatefulWidget {
  final GcsController controller;
  final JoystickController joystick;
  const HandheldScreen(
      {super.key, required this.controller, required this.joystick});

  @override
  State<HandheldScreen> createState() => _HandheldScreenState();
}

class _HandheldScreenState extends State<HandheldScreen> {
  String _view = 'MAP'; // MAP | HUD

  @override
  Widget build(BuildContext context) {
    return Scaffold(
      backgroundColor: AppTheme.bg,
      body: SafeArea(
        child: ListenableBuilder(
          listenable: widget.controller,
          builder: (context, _) {
            final t = widget.controller.telemetry;
            return Stack(
              children: [
                Column(
                  children: [
                    _StatusBar(telemetry: t),
                    Expanded(
                      child: Row(
                        children: [
                          Expanded(child: _mainView(t)),
                          SizedBox(
                            width: 150,
                            child: ModeRail(
                              controller: widget.controller,
                              joystick: widget.joystick,
                            ),
                          ),
                        ],
                      ),
                    ),
                    _TelemetryStrip(telemetry: t),
                  ],
                ),
                if (!t.connected)
                  Center(child: ConnectionCard(controller: widget.controller)),
              ],
            );
          },
        ),
      ),
    );
  }

  Widget _mainView(Telemetry t) {
    return Stack(
      children: [
        Positioned.fill(
          child: _view == 'MAP'
              ? MapView(controller: widget.controller)
              : HudView(telemetry: t),
        ),
        // nút đổi view (PiP nhỏ góc trên-phải)
        Positioned(
          top: 8,
          right: 8,
          child: _ViewToggle(
            current: _view,
            onTap: (v) => setState(() => _view = v),
          ),
        ),
        // cảnh báo (trên-giữa)
        Positioned(
          top: 46,
          left: 0,
          right: 0,
          child: Center(child: WarningBanners(controller: widget.controller)),
        ),
        // lớp phủ cần lái ảo
        Positioned.fill(child: JoystickOverlay(joystick: widget.joystick)),
      ],
    );
  }
}

class _ViewToggle extends StatelessWidget {
  final String current;
  final ValueChanged<String> onTap;
  const _ViewToggle({required this.current, required this.onTap});

  @override
  Widget build(BuildContext context) {
    final other = current == 'MAP' ? 'HUD' : 'MAP';
    return GestureDetector(
      onTap: () => onTap(other),
      child: Container(
        width: 120,
        height: 40,
        decoration: BoxDecoration(
          color: const Color(0xE6070A09),
          border: Border.all(color: const Color(0xFF2A312B)),
          borderRadius: BorderRadius.circular(8),
        ),
        alignment: Alignment.center,
        child: Text('▸ $other',
            style: const TextStyle(
                fontFamily: AppTheme.mono,
                color: AppTheme.accent,
                fontSize: 12,
                letterSpacing: 1)),
      ),
    );
  }
}

// ── thanh trạng thái ────────────────────────────────────────────────────────
class _StatusBar extends StatelessWidget {
  final Telemetry telemetry;
  const _StatusBar({required this.telemetry});

  @override
  Widget build(BuildContext context) {
    final t = telemetry;
    final armed = t.connected && t.armed;
    return Container(
      height: 38,
      padding: const EdgeInsets.symmetric(horizontal: 12),
      decoration: const BoxDecoration(
        color: AppTheme.panelSolid,
        border: Border(bottom: BorderSide(color: AppTheme.strokeSoft)),
      ),
      child: Row(
        children: [
          const Icon(Icons.flight, color: AppTheme.danger, size: 16),
          const SizedBox(width: 6),
          const Text('GROUND',
              style: TextStyle(
                  fontFamily: AppTheme.mono,
                  fontWeight: FontWeight.bold,
                  color: AppTheme.text,
                  fontSize: 13)),
          const Text('CTRL',
              style: TextStyle(
                  fontFamily: AppTheme.mono,
                  fontWeight: FontWeight.bold,
                  color: AppTheme.accent,
                  fontSize: 13)),
          const SizedBox(width: 16),
          _pill(
            armed ? 'ARMED' : 'CHƯA ARM',
            armed ? AppTheme.danger : AppTheme.dim,
          ),
          const SizedBox(width: 10),
          Text('CHẾ ĐỘ ${t.connected ? t.modeLabel : "—"}',
              style: const TextStyle(
                  fontFamily: AppTheme.ui, color: AppTheme.sub, fontSize: 11)),
          const Spacer(),
          Text(
              t.hasFix
                  ? 'GPS ${t.satellites}vt'
                  : 'GPS 0·Không GPS',
              style: TextStyle(
                  fontFamily: AppTheme.mono,
                  color: t.hasFix ? AppTheme.accent : AppTheme.dim,
                  fontSize: 11)),
        ],
      ),
    );
  }

  Widget _pill(String text, Color color) => Container(
        padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
        decoration: BoxDecoration(
          border: Border.all(color: color),
          borderRadius: BorderRadius.circular(4),
        ),
        child: Text(text,
            style: TextStyle(
                fontFamily: AppTheme.mono,
                color: color,
                fontSize: 10,
                fontWeight: FontWeight.bold)),
      );
}

// ── dải telemetry (dưới) ────────────────────────────────────────────────────
class _TelemetryStrip extends StatelessWidget {
  final Telemetry telemetry;
  const _TelemetryStrip({required this.telemetry});

  @override
  Widget build(BuildContext context) {
    final t = telemetry;
    final tiles = <List<String>>[
      ['TỐC ĐỘ ĐẤT', t.groundSpeed.toStringAsFixed(1), 'm/s'],
      ['CAO ĐỘ', t.altRel.toStringAsFixed(1), 'm'],
      ['K/C ĐIỂM VỀ', t.homeDistance.toStringAsFixed(0), 'm'],
      ['HƯỚNG', t.heading.toStringAsFixed(0), '°'],
      ['GPS', '${t.satellites}', 'vt'],
      ['PIN', t.voltage.toStringAsFixed(1), 'V'],
    ];
    return Container(
      height: 80,
      decoration: const BoxDecoration(
        color: AppTheme.panelSolid,
        border: Border(top: BorderSide(color: AppTheme.strokeSoft)),
      ),
      child: Row(
        children: tiles
            .map((e) => Expanded(child: _tile(e[0], e[1], e[2])))
            .toList(),
      ),
    );
  }

  Widget _tile(String label, String value, String unit) => Container(
        padding: const EdgeInsets.symmetric(horizontal: 10),
        decoration: const BoxDecoration(
          border: Border(right: BorderSide(color: AppTheme.strokeSoft)),
        ),
        child: Column(
          mainAxisAlignment: MainAxisAlignment.center,
          crossAxisAlignment: CrossAxisAlignment.start,
          children: [
            Text(label,
                style: const TextStyle(
                    fontFamily: AppTheme.ui,
                    color: AppTheme.dim,
                    fontSize: 9,
                    letterSpacing: 1)),
            const SizedBox(height: 4),
            Row(
              crossAxisAlignment: CrossAxisAlignment.baseline,
              textBaseline: TextBaseline.alphabetic,
              children: [
                Flexible(
                  child: Text(value,
                      overflow: TextOverflow.ellipsis,
                      style: const TextStyle(
                          fontFamily: AppTheme.mono,
                          color: AppTheme.text,
                          fontSize: 22,
                          fontWeight: FontWeight.bold)),
                ),
                const SizedBox(width: 3),
                Text(unit,
                    style: const TextStyle(
                        fontFamily: AppTheme.mono,
                        color: AppTheme.dim,
                        fontSize: 11)),
              ],
            ),
          ],
        ),
      );
}
