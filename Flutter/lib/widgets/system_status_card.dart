import 'package:flutter/material.dart';

import '../theme/app_colors.dart';
import 'section_card.dart';

/// 2x2 grid of device-level status indicators: Bluetooth, GPS,
/// connection, and (always-on) security.
class SystemStatusCard extends StatelessWidget {
  final bool bluetoothEnabled;
  final bool gpsActive;
  final bool connectionActive;

  const SystemStatusCard({
    super.key,
    required this.bluetoothEnabled,
    required this.gpsActive,
    required this.connectionActive,
  });

  @override
  Widget build(BuildContext context) {
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          const Row(
            children: [
              Icon(Icons.bar_chart, color: AppColors.primary, size: 20),
              SizedBox(width: 8),
              Text(
                'System Status',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
            ],
          ),
          const SizedBox(height: 12),
          Row(
            children: [
              Expanded(
                child: _StatusTile(
                  icon: Icons.bluetooth,
                  label: 'Bluetooth',
                  value: bluetoothEnabled ? 'Enabled' : 'Disabled',
                  active: bluetoothEnabled,
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _StatusTile(
                  icon: Icons.location_on,
                  label: 'GPS',
                  value: gpsActive ? 'Active' : 'Off',
                  active: gpsActive,
                ),
              ),
            ],
          ),
          const SizedBox(height: 10),
          Row(
            children: [
              Expanded(
                child: _StatusTile(
                  icon: Icons.wifi,
                  label: 'Connection',
                  value: connectionActive ? 'Active' : 'Inactive',
                  active: connectionActive,
                ),
              ),
              const SizedBox(width: 10),
              const Expanded(
                child: _StatusTile(
                  icon: Icons.verified_user,
                  label: 'Security',
                  value: 'Secure',
                  active: true,
                ),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _StatusTile extends StatelessWidget {
  final IconData icon;
  final String label;
  final String value;
  final bool active;

  const _StatusTile({
    required this.icon,
    required this.label,
    required this.value,
    required this.active,
  });

  @override
  Widget build(BuildContext context) {
    final color = active ? AppColors.statusActive : AppColors.muted;
    return Container(
      padding: const EdgeInsets.symmetric(vertical: 12),
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Column(
        children: [
          Icon(icon, color: color, size: 22),
          const SizedBox(height: 6),
          Text(label, style: const TextStyle(fontSize: 11, color: AppColors.muted)),
          Text(
            value,
            style: TextStyle(fontSize: 12, fontWeight: FontWeight.bold, color: color),
          ),
        ],
      ),
    );
  }
}
