import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';
import 'section_card.dart';

/// Scan trigger, empty state, and the list of nearby jacket devices.
class AvailableDevicesCard extends StatelessWidget {
  final bool isScanning;
  final List<ScanResult> results;
  final VoidCallback onScan;
  final ValueChanged<BluetoothDevice> onConnect;
  final bool isConnecting;

  const AvailableDevicesCard({
    super.key,
    required this.isScanning,
    required this.results,
    required this.onScan,
    required this.onConnect,
    required this.isConnecting,
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
              const Icon(Icons.bluetooth, color: AppColors.primary, size: 20),
              const SizedBox(width: 8),
              Text(
                s.availableDevices,
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
              const Spacer(),
              TextButton(
                onPressed: isScanning ? null : onScan,
                child: Text(isScanning ? s.scanningEllipsis : s.scanAgain),
              ),
            ],
          ),
          if (results.isEmpty) ...[
            const SizedBox(height: 12),
            Center(
              child: Column(
                children: [
                  Icon(
                    Icons.bluetooth_disabled,
                    size: 48,
                    color: AppColors.mutedLight,
                  ),
                  const SizedBox(height: 12),
                  Text(
                    s.noDevicesFound,
                    style: const TextStyle(fontWeight: FontWeight.bold),
                  ),
                  const SizedBox(height: 4),
                  Text(
                    s.clickScanToSearch,
                    textAlign: TextAlign.center,
                    style: const TextStyle(fontSize: 12, color: AppColors.muted),
                  ),
                  const SizedBox(height: 16),
                  SizedBox(
                    width: double.infinity,
                    child: ElevatedButton.icon(
                      onPressed: isScanning ? null : onScan,
                      icon: const Icon(Icons.search),
                      label: Text(s.startScanning),
                      style: ElevatedButton.styleFrom(
                        backgroundColor: AppColors.accentBlue,
                        foregroundColor: Colors.white,
                        padding: const EdgeInsets.symmetric(vertical: 12),
                      ),
                    ),
                  ),
                ],
              ),
            ),
          ] else ...[
            const SizedBox(height: 8),
            for (final result in results)
              Padding(
                padding: const EdgeInsets.only(top: 8),
                child: _DeviceTile(
                  result: result,
                  connecting: isConnecting,
                  onConnect: () => onConnect(result.device),
                ),
              ),
          ],
        ],
      ),
    );
  }
}

class _DeviceTile extends StatelessWidget {
  final ScanResult result;
  final bool connecting;
  final VoidCallback onConnect;

  const _DeviceTile({
    required this.result,
    required this.connecting,
    required this.onConnect,
  });

  @override
  Widget build(BuildContext context) {
    final name = result.device.platformName.isNotEmpty
        ? result.device.platformName
        : (result.advertisementData.advName.isNotEmpty
            ? result.advertisementData.advName
            : 'Smart Jacket');

    return Container(
      padding: const EdgeInsets.all(10),
      decoration: BoxDecoration(
        color: AppColors.background,
        borderRadius: BorderRadius.circular(12),
      ),
      child: Row(
        children: [
          const Icon(Icons.bluetooth, color: AppColors.accentBlue),
          const SizedBox(width: 10),
          Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(name, style: const TextStyle(fontWeight: FontWeight.w600)),
                Text(
                  result.device.remoteId.str,
                  style: const TextStyle(fontSize: 11, color: AppColors.muted),
                ),
                Text(
                  'RSSI: ${result.rssi} dBm',
                  style: const TextStyle(fontSize: 11, color: AppColors.muted),
                ),
              ],
            ),
          ),
          IconButton.filled(
            onPressed: connecting ? null : onConnect,
            icon: const Icon(Icons.link),
            style: IconButton.styleFrom(backgroundColor: AppColors.accentBlue),
          ),
        ],
      ),
    );
  }
}
