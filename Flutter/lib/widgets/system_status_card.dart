import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';
import 'section_card.dart';

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
    final s = Strings(context.watch<LocaleService>().language);
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.bar_chart, color: AppColors.primary, size: 20),
              const SizedBox(width: 8),
              Text(
                s.systemStatus,
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
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
                  value: bluetoothEnabled ? s.enabled : s.disabled,
                  active: bluetoothEnabled,
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _StatusTile(
                  icon: Icons.location_on,
                  label: 'GPS',
                  value: gpsActive ? s.active : s.off,
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
                  label: s.connectionLabelShort,
                  value: connectionActive ? s.active : s.inactive,
                  active: connectionActive,
                ),
              ),
              const SizedBox(width: 10),
              Expanded(
                child: _StatusTile(
                  icon: Icons.verified_user,
                  label: s.security,
                  value: s.secure,
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
