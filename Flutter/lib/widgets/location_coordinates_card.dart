import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../models/lora_node.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';
import 'section_card.dart';

class LocationCoordinatesCard extends StatelessWidget {
  final double? myLatitude;
  final double? myLongitude;
  final bool gpsActive;
  final List<LoraNode> nodes;

  const LocationCoordinatesCard({
    super.key,
    required this.myLatitude,
    required this.myLongitude,
    required this.gpsActive,
    required this.nodes,
  });

  @override
  Widget build(BuildContext context) {
    final s = Strings(context.watch<LocaleService>().language);
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.push_pin, color: AppColors.primary, size: 20),
              const SizedBox(width: 8),
              Text(
                s.locationCoordinates,
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
            ],
          ),
          const SizedBox(height: 12),
          LayoutBuilder(
            builder: (context, constraints) {
              const spacing = 12.0;
              final tileWidth = (constraints.maxWidth - spacing) / 2;
              final tileCount = 1 + nodes.length;
              final lastIsOdd = tileCount.isOdd;
              Widget tileAt(int i) {
                final width = lastIsOdd && i == tileCount - 1 ? constraints.maxWidth : tileWidth;
                if (i == 0) {
                  return SizedBox(
                    width: width,
                    child: _CoordinateTile(
                      icon: Icons.my_location,
                      iconColor: AppColors.accentBlue,
                      title: s.yourLocation,
                      latitude: myLatitude,
                      longitude: myLongitude,
                      pillLabel: gpsActive ? s.gpsActive : s.gpsOff,
                      pillActive: gpsActive,
                      highlighted: true,
                    ),
                  );
                }
                final node = nodes[i - 1];
                return SizedBox(
                  width: width,
                  child: _CoordinateTile(
                    icon: Icons.location_on,
                    iconColor: node.hasFix ? AppColors.warning : AppColors.mutedLight,
                    title: node.name,
                    latitude: node.latitude,
                    longitude: node.longitude,
                    pillLabel: node.hasFix ? s.live : s.noFix,
                    pillActive: node.hasFix,
                  ),
                );
              }

              return Wrap(
                spacing: spacing,
                runSpacing: spacing,
                children: [for (var i = 0; i < tileCount; i++) tileAt(i)],
              );
            },
          ),
          if (nodes.isEmpty) ...[
            const SizedBox(height: 12),
            Container(
              width: double.infinity,
              padding: const EdgeInsets.symmetric(vertical: 16),
              decoration: BoxDecoration(
                color: AppColors.background,
                borderRadius: BorderRadius.circular(14),
                border: Border.all(color: AppColors.mutedLight, width: 1),
              ),
              child: Column(
                children: [
                  const Icon(Icons.satellite_alt, size: 22, color: AppColors.mutedLight),
                  const SizedBox(height: 6),
                  Text(
                    s.noLoraReportingYet,
                    style: const TextStyle(fontSize: 12, color: AppColors.muted),
                  ),
                ],
              ),
            ),
          ],
        ],
      ),
    );
  }
}

class _CoordinateTile extends StatelessWidget {
  final IconData icon;
  final Color iconColor;
  final String title;
  final double? latitude;
  final double? longitude;
  final String pillLabel;
  final bool pillActive;
  final bool highlighted;

  const _CoordinateTile({
    required this.icon,
    required this.iconColor,
    required this.title,
    required this.latitude,
    required this.longitude,
    required this.pillLabel,
    required this.pillActive,
    this.highlighted = false,
  });

  @override
  Widget build(BuildContext context) {
    final statusColor = pillActive ? AppColors.statusActive : AppColors.muted;

    return Container(
      width: double.infinity,
      padding: const EdgeInsets.all(12),
      decoration: BoxDecoration(
        color: highlighted ? AppColors.accentBlue.withValues(alpha: 0.08) : AppColors.background,
        borderRadius: BorderRadius.circular(14),
        border: highlighted
            ? Border.all(color: AppColors.accentBlue.withValues(alpha: 0.25))
            : null,
      ),
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              CircleAvatar(
                radius: 14,
                backgroundColor: iconColor.withValues(alpha: 0.15),
                child: Icon(icon, size: 15, color: iconColor),
              ),
              const SizedBox(width: 8),
              Expanded(
                child: Text(
                  title,
                  overflow: TextOverflow.ellipsis,
                  style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 13),
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          _CoordinateRow(label: 'LAT', value: latitude),
          const SizedBox(height: 3),
          _CoordinateRow(label: 'LON', value: longitude),
          const SizedBox(height: 10),
          Container(
            padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
            decoration: BoxDecoration(
              color: statusColor.withValues(alpha: 0.12),
              borderRadius: BorderRadius.circular(20),
            ),
            child: Row(
              mainAxisSize: MainAxisSize.min,
              children: [
                Container(
                  width: 6,
                  height: 6,
                  decoration: BoxDecoration(color: statusColor, shape: BoxShape.circle),
                ),
                const SizedBox(width: 5),
                Text(
                  pillLabel,
                  style: TextStyle(fontSize: 10, fontWeight: FontWeight.bold, color: statusColor),
                ),
              ],
            ),
          ),
        ],
      ),
    );
  }
}

class _CoordinateRow extends StatelessWidget {
  final String label;
  final double? value;

  const _CoordinateRow({required this.label, required this.value});

  @override
  Widget build(BuildContext context) {
    return Row(
      children: [
        SizedBox(
          width: 26,
          child: Text(
            label,
            style: const TextStyle(fontSize: 10, color: AppColors.muted, fontWeight: FontWeight.w600),
          ),
        ),
        Expanded(
          child: Text(
            value?.toStringAsFixed(6) ?? '--',
            style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 13, letterSpacing: 0.2),
          ),
        ),
      ],
    );
  }
}
