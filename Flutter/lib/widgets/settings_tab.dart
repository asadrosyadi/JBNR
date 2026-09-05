import 'package:flutter/material.dart';
import 'package:flutter_blue_plus/flutter_blue_plus.dart';

import 'available_devices_card.dart';
import 'device_info_card.dart';
import 'language_card.dart';
import 'offline_map_cache_card.dart';
import 'system_status_card.dart';

class SettingsTab extends StatelessWidget {
  final bool isConnected;
  final bool isScanning;
  final bool isConnecting;
  final List<ScanResult> scanResults;
  final VoidCallback onScan;
  final ValueChanged<BluetoothDevice> onConnect;
  final String deviceName;
  final String deviceId;
  final VoidCallback onDisconnect;
  final bool bluetoothEnabled;
  final bool gpsActive;

  const SettingsTab({
    super.key,
    required this.isConnected,
    required this.isScanning,
    required this.isConnecting,
    required this.scanResults,
    required this.onScan,
    required this.onConnect,
    required this.deviceName,
    required this.deviceId,
    required this.onDisconnect,
    required this.bluetoothEnabled,
    required this.gpsActive,
  });

  @override
  Widget build(BuildContext context) {
    return ListView(
      padding: const EdgeInsets.all(16),
      children: [
        if (isConnected) ...[
          DeviceInfoCard(
            deviceName: deviceName,
            deviceId: deviceId,
            onDisconnect: onDisconnect,
          ),
          const SizedBox(height: 16),
        ] else ...[
          AvailableDevicesCard(
            isScanning: isScanning,
            results: scanResults,
            onScan: onScan,
            onConnect: onConnect,
            isConnecting: isConnecting,
          ),
          const SizedBox(height: 16),
        ],
        SystemStatusCard(
          bluetoothEnabled: bluetoothEnabled,
          gpsActive: gpsActive,
          connectionActive: isConnected,
        ),
        const SizedBox(height: 16),
        const LanguageCard(),
        const SizedBox(height: 16),
        const OfflineMapCacheCard(),
      ],
    );
  }
}
