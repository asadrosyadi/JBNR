import 'package:flutter/material.dart';

import '../models/lora_node.dart';
import '../theme/app_colors.dart';
import 'section_card.dart';

/// "Your Location" readout plus a coordinate tile for every connected
/// LoRa node - always all of them, independent of the node picked in
/// [NodeSelectorCard]. "Your Location" is always shown full-width and
/// visually distinct (it's the one fixed reference point everything
/// else is measured against); node tiles fill a responsive 2-column
/// grid below it.
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
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Row(
            children: [
              Icon(Icons.push_pin, color: AppColors.primary, size: 20),
              SizedBox(width: 8),
              Text(
                'Location Coordinates',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
            ],
          ),
          const SizedBox(height: 12),
          _CoordinateTile(
            icon: Icons.my_location,
            iconColor: AppColors.accentBlue,
            title: 'Your Location',
            latitude: myLatitude,
            longitude: myLongitude,
            pillLabel: gpsActive ? 'GPS Active' : 'GPS Off',
            pillActive: gpsActive,
            highlighted: true,
          ),
          const SizedBox(height: 12),
          if (nodes.isEmpty)
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
                  Icon(Icons.satellite_alt, size: 22, color: AppColors.mutedLight),
                  const SizedBox(height: 6),
                  const Text(
                    'No LoRa nodes reporting yet',
                    style: TextStyle(fontSize: 12, color: AppColors.muted),
                  ),
                ],
              ),
            )
          else
            LayoutBuilder(
              builder: (context, constraints) {
                const spacing = 12.0;
                final tileWidth = (constraints.maxWidth - spacing) / 2;
                return Wrap(
                  spacing: spacing,
                  runSpacing: spacing,
                  children: [
                    for (final node in nodes)
                      SizedBox(
                        width: tileWidth,
                        child: _CoordinateTile(
                          icon: Icons.location_on,
                          iconColor: node.hasFix ? AppColors.warning : AppColors.mutedLight,
                          title: node.name,
                          latitude: node.latitude,
                          longitude: node.longitude,
                          pillLabel: node.hasFix ? 'Live' : 'No fix',
                          pillActive: node.hasFix,
                        ),
                      ),
                  ],
                );
              },
            ),
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
