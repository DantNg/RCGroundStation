// Banner cảnh báo — hiện thông báo STATUSTEXT gần nhất (severity ≤ 4). Port WarningBanners.qml.
import 'package:flutter/material.dart';

import '../controller.dart';
import '../theme.dart';

class WarningBanners extends StatelessWidget {
  final GcsController controller;
  const WarningBanners({super.key, required this.controller});

  @override
  Widget build(BuildContext context) {
    // 3 thông báo nặng nhất gần đây
    final recent = controller.notices
        .where((n) => n.severity <= 4)
        .toList()
        .reversed
        .take(3)
        .toList();
    if (recent.isEmpty) return const SizedBox.shrink();
    return Column(
      mainAxisSize: MainAxisSize.min,
      children: recent.map((n) {
        final color = n.isCritical ? AppTheme.danger : AppTheme.warn;
        return Container(
          margin: const EdgeInsets.only(bottom: 4),
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 6),
          constraints: const BoxConstraints(maxWidth: 480),
          decoration: BoxDecoration(
            color: const Color(0xE60A0D0B),
            border: Border.all(color: color),
            borderRadius: BorderRadius.circular(6),
          ),
          child: Row(
            mainAxisSize: MainAxisSize.min,
            children: [
              Icon(n.isCritical ? Icons.error : Icons.warning,
                  color: color, size: 14),
              const SizedBox(width: 8),
              Flexible(
                child: Text(n.text,
                    maxLines: 2,
                    overflow: TextOverflow.ellipsis,
                    style: TextStyle(
                        fontFamily: AppTheme.mono, color: color, fontSize: 10)),
              ),
            ],
          ),
        );
      }).toList(),
    );
  }
}
