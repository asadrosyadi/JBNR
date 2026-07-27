import 'package:flutter/material.dart';

import '../theme/app_colors.dart';

class _StatusDot extends StatelessWidget {
  final Color color;
  final String label;

  const _StatusDot({required this.color, required this.label});

  @override
  Widget build(BuildContext context) {
    return Row(
      mainAxisSize: MainAxisSize.min,
      children: [
        Container(
          width: 8,
          height: 8,
          decoration: BoxDecoration(color: color, shape: BoxShape.circle),
        ),
        const SizedBox(width: 6),
        Text(
          label,
          style: const TextStyle(fontSize: 12, color: Colors.white),
        ),
      ],
    );
  }
}

/// Row of three status indicators: connection, GPS, and how many
/// jacket devices are currently visible in the scan. Sits at the dark
/// end of the header gradient, so labels are light and only the dot
/// color communicates active/inactive.
class StatusBar extends StatelessWidget {
  final bool connected;
  final bool connecting;
  final bool gpsActive;
  final int deviceCount;

  const StatusBar({
    super.key,
    required this.connected,
    required this.connecting,
    required this.gpsActive,
    required this.deviceCount,
  });

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 0, 16, 16),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          _StatusDot(
            color: connected
                ? AppColors.statusActive
                : (connecting ? AppColors.warning : AppColors.statusInactiveOnDark),
            label: connected
                ? 'Connected'
                : (connecting ? 'Connecting...' : 'Disconnected'),
          ),
          _StatusDot(
            color: gpsActive ? AppColors.statusActive : AppColors.statusInactiveOnDark,
            label: gpsActive ? 'GPS Active' : 'GPS Off',
          ),
          _StatusDot(
            color: AppColors.statusInactiveOnDark,
            label: '$deviceCount Devices',
          ),
        ],
      ),
    );
  }
}
