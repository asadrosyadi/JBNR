import 'package:flutter/material.dart';

import '../theme/app_colors.dart';
import 'section_card.dart';

/// Heart rate, SpO2, and temperature readings from a jacket. Reused
/// for both the "My Jacket" pin and the "All LoRa Readings" browser -
/// [title] and [trailing] (typically a node-picker dropdown) tell them
/// apart.
class HealthMetricsCard extends StatelessWidget {
  final double? heartRate;
  final double? spo2;
  final double? temperature;
  final String title;
  final Widget? trailing;

  const HealthMetricsCard({
    super.key,
    required this.heartRate,
    required this.spo2,
    required this.temperature,
    this.title = 'Health Metrics',
    this.trailing,
  });

  @override
  Widget build(BuildContext context) {
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.favorite, color: AppColors.danger, size: 20),
              const SizedBox(width: 8),
              Text(
                title,
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
              if (trailing != null) ...[
                const Spacer(),
                trailing!,
              ],
            ],
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: _MetricTile(
                  icon: Icons.favorite,
                  iconColor: AppColors.danger,
                  value: heartRate?.toStringAsFixed(1) ?? '--',
                  label: 'Heart Rate\nBPM',
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _MetricTile(
                  icon: Icons.water_drop,
                  iconColor: AppColors.dangerDark,
                  value: spo2?.toStringAsFixed(1) ?? '--',
                  label: 'SpO2\n%',
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _MetricTile(
                  icon: Icons.thermostat,
                  iconColor: AppColors.warning,
                  value: temperature?.toStringAsFixed(1) ?? '--',
                  label: 'Temperature\n°C',
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _MetricTile extends StatelessWidget {
  final IconData icon;
  final Color iconColor;
  final String value;
  final String label;

  const _MetricTile({
    required this.icon,
    required this.iconColor,
    required this.value,
    required this.label,
  });

  @override
  Widget build(BuildContext context) {
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 14, horizontal: 6),
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        children: [
          CircleAvatar(
            radius: 16,
            backgroundColor: iconColor.withValues(alpha: 0.15),
            child: Icon(icon, color: iconColor, size: 18),
          ),
          const SizedBox(height: 8),
          Text(
            value,
            style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 17),
          ),
          const SizedBox(height: 2),
          Text(
            label,
            textAlign: TextAlign.center,
            style: const TextStyle(fontSize: 10, color: AppColors.muted, height: 1.2),
          ),
        ],
      ),
    );
  }
}
