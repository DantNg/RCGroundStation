// Bản đồ 2D vệ tinh (Esri World Imagery) + marker phương tiện + chạm-để-bay-đến.
// Port SatelliteMap.qml sang flutter_map.
import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';

import '../controller.dart';
import '../theme.dart';

class MapView extends StatefulWidget {
  final GcsController controller;
  const MapView({super.key, required this.controller});

  @override
  State<MapView> createState() => _MapViewState();
}

class _MapViewState extends State<MapView> {
  final MapController _map = MapController();
  LatLng? _target;
  bool _follow = true;

  @override
  Widget build(BuildContext context) {
    final t = widget.controller.telemetry;
    final hasPos = t.lat != 0 || t.lon != 0;
    final drone = LatLng(hasPos ? t.lat : 21.0285, hasPos ? t.lon : 105.8048);

    if (_follow && hasPos) {
      WidgetsBinding.instance.addPostFrameCallback((_) {
        try {
          _map.move(drone, _map.camera.zoom);
        } catch (_) {}
      });
    }

    return Stack(
      children: [
        FlutterMap(
          mapController: _map,
          options: MapOptions(
            initialCenter: drone,
            initialZoom: 17,
            maxZoom: 19,
            onPointerDown: (_, _) => _follow = false,
            onTap: (_, ll) => setState(() => _target = ll),
          ),
          children: [
            TileLayer(
              urlTemplate:
                  'https://server.arcgisonline.com/ArcGIS/rest/services/World_Imagery/MapServer/tile/{z}/{y}/{x}',
              userAgentPackageName: 'com.gcs.gcs_mobile',
              maxZoom: 19,
            ),
            MarkerLayer(markers: [
              Marker(
                point: drone,
                width: 34,
                height: 34,
                child: Transform.rotate(
                  angle: t.heading * 3.14159265 / 180,
                  child: const Icon(Icons.navigation,
                      color: AppTheme.danger, size: 30),
                ),
              ),
              if (_target != null)
                Marker(
                  point: _target!,
                  width: 22,
                  height: 22,
                  child: const Icon(Icons.add_location,
                      color: AppTheme.accent, size: 22),
                ),
            ]),
          ],
        ),
        // nút GOTO / định tâm
        Positioned(
          left: 8,
          bottom: 8,
          child: Row(
            children: [
              if (_target != null)
                _btn('▸ BAY ĐẾN', AppTheme.accent, () {
                  widget.controller
                      .flyTo(_target!.latitude, _target!.longitude, t.altRel);
                  setState(() => _target = null);
                }),
              if (!_follow) ...[
                const SizedBox(width: 8),
                _btn('◎ ĐỊNH TÂM', AppTheme.sub, () {
                  setState(() => _follow = true);
                }),
              ],
            ],
          ),
        ),
      ],
    );
  }

  Widget _btn(String label, Color tint, VoidCallback onTap) => GestureDetector(
        onTap: onTap,
        child: Container(
          padding: const EdgeInsets.symmetric(horizontal: 12, vertical: 8),
          decoration: BoxDecoration(
            color: const Color(0xE6070A09),
            border: Border.all(color: tint),
            borderRadius: BorderRadius.circular(8),
          ),
          child: Text(label,
              style: TextStyle(
                  fontFamily: AppTheme.mono,
                  color: tint,
                  fontSize: 12,
                  fontWeight: FontWeight.bold)),
        ),
      );
}
