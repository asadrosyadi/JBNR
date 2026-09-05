import 'package:flutter/material.dart';
import 'package:flutter_map/flutter_map.dart';
import 'package:latlong2/latlong.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../models/lora_node.dart';
import '../screens/full_map_screen.dart';
import '../services/locale_service.dart';
import '../services/map_tile_downloader.dart';
import '../services/offline_first_tile_provider.dart';
import '../theme/app_colors.dart';

class LiveMapCard extends StatelessWidget {
  final double? myLatitude;
  final double? myLongitude;
  final List<LoraNode> nodes;

  const LiveMapCard({
    super.key,
    required this.myLatitude,
    required this.myLongitude,
    required this.nodes,
  });

  List<LatLng> get _points => [
        if (myLatitude != null && myLongitude != null) LatLng(myLatitude!, myLongitude!),
        for (final node in nodes)
          if (node.hasFix) LatLng(node.latitude!, node.longitude!),
      ];

  @override
  Widget build(BuildContext context) {
    final points = _points;
    if (points.isEmpty) {
      final s = Strings(context.watch<LocaleService>().language);
      return Container(
        height: 220,
        alignment: Alignment.center,
        decoration: BoxDecoration(
          color: AppColors.background,
          borderRadius: BorderRadius.circular(10),
        ),
        child: Text(
          s.waitingGpsFix,
          style: const TextStyle(color: AppColors.muted, fontSize: 12),
        ),
      );
    }

    return GestureDetector(
      onTap: () => Navigator.of(context).push(
        MaterialPageRoute(builder: (_) => const FullMapScreen()),
      ),
      child: ClipRRect(
        borderRadius: BorderRadius.circular(10),
        child: SizedBox(
          height: 220,
          child: Stack(
            children: [
              FlutterMap(
                options: MapOptions(
                  initialCenter: points.first,
                  initialZoom: 15,
                  initialCameraFit: points.length > 1
                      ? CameraFit.bounds(
                          bounds: LatLngBounds.fromPoints(points),
                          padding: const EdgeInsets.all(24),
                        )
                      : null,
                  interactionOptions: const InteractionOptions(flags: InteractiveFlag.none),
                ),
                children: [
                  TileLayer(
                    urlTemplate: tileUrlTemplate,
                    userAgentPackageName: tileUserAgentPackageName,
                    tileProvider: OfflineFirstTileProvider(),
                    errorTileCallback: (tile, error, stackTrace) =>
                        debugPrint('[LiveMapCard] Tile load failed: $error'),
                  ),
                  MarkerLayer(
                    markers: [
                      if (myLatitude != null && myLongitude != null)
                        Marker(
                          point: LatLng(myLatitude!, myLongitude!),
                          width: 36,
                          height: 36,
                          child: const Icon(Icons.my_location, color: AppColors.accentBlue, size: 28),
                        ),
                      for (final node in nodes)
                        if (node.hasFix)
                          Marker(
                            point: LatLng(node.latitude!, node.longitude!),
                            width: 36,
                            height: 36,
                            child: const Icon(Icons.location_on, color: AppColors.warning, size: 32),
                          ),
                    ],
                  ),
                ],
              ),
              Positioned(
                right: 8,
                bottom: 8,
                child: Container(
                  padding: const EdgeInsets.all(6),
                  decoration: BoxDecoration(
                    color: Colors.black.withValues(alpha: 0.55),
                    borderRadius: BorderRadius.circular(8),
                  ),
                  child: const Icon(Icons.fullscreen, color: Colors.white, size: 18),
                ),
              ),
            ],
          ),
        ),
      ),
    );
  }
}
