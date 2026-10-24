// Thẻ kết nối (giữa màn khi chưa kết nối) — nhập UDP host/cổng rồi KẾT NỐI.
// Port thu gọn ConnectionCard.qml (chỉ UDP cho mobile — Wi-Fi telemetry).
import 'package:flutter/material.dart';
import 'package:flutter/services.dart';

import '../controller.dart';
import '../theme.dart';

class ConnectionCard extends StatefulWidget {
  final GcsController controller;
  const ConnectionCard({super.key, required this.controller});

  @override
  State<ConnectionCard> createState() => _ConnectionCardState();
}

class _ConnectionCardState extends State<ConnectionCard> {
  final _host = TextEditingController();
  final _port = TextEditingController(text: '14550');
  bool _busy = false;

  @override
  void dispose() {
    _host.dispose();
    _port.dispose();
    super.dispose();
  }

  Future<void> _connect() async {
    setState(() => _busy = true);
    try {
      await widget.controller.connectUdp(
        host: _host.text.trim(),
        port: int.tryParse(_port.text.trim()) ?? 14550,
      );
    } finally {
      if (mounted) setState(() => _busy = false);
    }
  }

  @override
  Widget build(BuildContext context) {
    return Container(
      width: 360,
      padding: const EdgeInsets.all(20),
      decoration: BoxDecoration(
        color: const Color(0xF20A0D0B),
        border: Border.all(color: AppTheme.stroke),
        borderRadius: BorderRadius.circular(14),
      ),
      child: Column(
        mainAxisSize: MainAxisSize.min,
        crossAxisAlignment: CrossAxisAlignment.stretch,
        children: [
          const Row(
            children: [
              Text('GROUND',
                  style: TextStyle(
                      fontFamily: AppTheme.mono,
                      color: AppTheme.text,
                      fontSize: 20,
                      fontWeight: FontWeight.bold)),
              Text('CTRL',
                  style: TextStyle(
                      fontFamily: AppTheme.mono,
                      color: AppTheme.accent,
                      fontSize: 20,
                      fontWeight: FontWeight.bold)),
              Spacer(),
              Text('CHƯA KẾT NỐI',
                  style: TextStyle(
                      fontFamily: AppTheme.ui,
                      color: AppTheme.dim,
                      fontSize: 11)),
            ],
          ),
          const SizedBox(height: 16),
          _field('IP máy bay / bộ phát (trống = nghe cổng)', _host,
              hint: 'vd 192.168.4.1 — trống để nghe'),
          const SizedBox(height: 10),
          _field('Cổng UDP', _port,
              numeric: true, hint: '14550'),
          const SizedBox(height: 16),
          GestureDetector(
            onTap: _busy ? null : _connect,
            child: Container(
              height: 46,
              decoration: BoxDecoration(
                color: AppTheme.accent.withValues(alpha: 0.16),
                border: Border.all(color: AppTheme.accent),
                borderRadius: BorderRadius.circular(8),
              ),
              alignment: Alignment.center,
              child: Text(_busy ? 'ĐANG KẾT NỐI…' : 'KẾT NỐI',
                  style: const TextStyle(
                      fontFamily: AppTheme.mono,
                      color: AppTheme.accent,
                      fontSize: 15,
                      fontWeight: FontWeight.bold,
                      letterSpacing: 1)),
            ),
          ),
        ],
      ),
    );
  }

  Widget _field(String label, TextEditingController c,
      {bool numeric = false, String? hint}) {
    return Column(
      crossAxisAlignment: CrossAxisAlignment.start,
      children: [
        Text(label,
            style: const TextStyle(
                fontFamily: AppTheme.ui, color: AppTheme.dim, fontSize: 10)),
        const SizedBox(height: 4),
        TextField(
          controller: c,
          keyboardType: numeric ? TextInputType.number : TextInputType.text,
          inputFormatters:
              numeric ? [FilteringTextInputFormatter.digitsOnly] : null,
          style: const TextStyle(
              fontFamily: AppTheme.mono, color: AppTheme.text, fontSize: 16),
          decoration: InputDecoration(
            isDense: true,
            hintText: hint,
            hintStyle: const TextStyle(color: AppTheme.dim, fontSize: 12),
            filled: true,
            fillColor: AppTheme.panel,
            contentPadding:
                const EdgeInsets.symmetric(horizontal: 12, vertical: 12),
            border: OutlineInputBorder(
              borderRadius: BorderRadius.circular(8),
              borderSide: const BorderSide(color: AppTheme.stroke),
            ),
            enabledBorder: OutlineInputBorder(
              borderRadius: BorderRadius.circular(8),
              borderSide: const BorderSide(color: AppTheme.stroke),
            ),
          ),
        ),
      ],
    );
  }
}
