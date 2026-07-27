import 'package:flutter/material.dart';

import '../theme/app_colors.dart';
import 'section_card.dart';

/// Connected-device details and the disconnect action. Messaging with
/// the LoRa nodes lives in its own dedicated Chat screen (see
/// [AppDrawer]) rather than inline here.
class DeviceInfoCard extends StatelessWidget {
  final String deviceName;
  final String deviceId;
  final VoidCallback onDisconnect;

  const DeviceInfoCard({
    super.key,
    required this.deviceName,
    required this.deviceId,
    required this.onDisconnect,
  });

  @override
  Widget build(BuildContext context) {
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.devices, color: AppColors.primary, size: 20),
              const SizedBox(width: 8),
              const Text(
                'Device Information',
                style: TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
              const Spacer(),
              Container(
                padding: const EdgeInsets.symmetric(horizontal: 8, vertical: 3),
                decoration: BoxDecoration(
                  color: AppColors.statusActive.withValues(alpha: 0.12),
                  borderRadius: BorderRadius.circular(20),
                ),
                child: const Row(
                  mainAxisSize: MainAxisSize.min,
                  children: [
                    Icon(Icons.circle, size: 8, color: AppColors.statusActive),
                    SizedBox(width: 4),
                    Text(
                      'ONLINE',
                      style: TextStyle(
                        fontSize: 10,
                        fontWeight: FontWeight.bold,
                        color: AppColors.statusActive,
                      ),
                    ),
                  ],
                ),
              ),
            ],
          ),
          const SizedBox(height: 12),
          _InfoRow(label: 'Device Name:', value: deviceName),
          _InfoRow(label: 'Device ID:', value: deviceId),
          const _InfoRow(
            label: 'Connection:',
            value: 'Secure',
            valueColor: AppColors.statusActive,
          ),
          const SizedBox(height: 16),
          SizedBox(
            width: double.infinity,
            child: ElevatedButton.icon(
              onPressed: onDisconnect,
              icon: const Icon(Icons.power_settings_new),
              label: const Text('Disconnect Device'),
              style: ElevatedButton.styleFrom(
                backgroundColor: AppColors.danger,
                foregroundColor: Colors.white,
                padding: const EdgeInsets.symmetric(vertical: 12),
              ),
            ),
          ),
        ],
      ),
    );
  }
}

class _InfoRow extends StatelessWidget {
  final String label;
  final String value;
  final Color? valueColor;

  const _InfoRow({required this.label, required this.value, this.valueColor});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 4),
      child: Row(
        mainAxisAlignment: MainAxisAlignment.spaceBetween,
        children: [
          Text(label, style: const TextStyle(fontSize: 13, color: AppColors.muted)),
          Text(
            value,
            style: TextStyle(
              fontSize: 13,
              fontWeight: FontWeight.w600,
              color: valueColor ?? AppColors.primaryDark,
            ),
          ),
        ],
      ),
    );
  }
}
