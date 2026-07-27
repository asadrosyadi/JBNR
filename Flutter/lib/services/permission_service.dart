import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';

/// Requests Bluetooth + Location runtime permissions and confirms the
/// underlying system services are switched on, surfacing the same
/// alert copy found in the original Smart Jacket app.
class PermissionService {
  /// Requests just the Location permission (not Bluetooth), so the
  /// phone's own GPS position on the home screen's Location Tracking
  /// card can populate immediately - it shouldn't have to wait for the
  /// user to scan for or connect to a jacket first.
  static Future<bool> ensureLocationReady(BuildContext context) async {
    final status = await Permission.locationWhenInUse.request();
    if (status.isDenied || status.isPermanentlyDenied) {
      if (!context.mounted) return false;
      await _showAlert(
        context,
        title: 'Location Permission Required',
        message: 'Location permission was denied. '
            'Please grant it so your GPS position can be shown.',
      );
      return false;
    }

    final serviceStatus = await Permission.location.serviceStatus;
    if (serviceStatus != ServiceStatus.enabled) {
      if (!context.mounted) return false;
      await _showAlert(
        context,
        title: 'Location Services Disabled',
        message: 'Location information is unavailable. '
            'Please check your GPS and network connection.',
      );
      return false;
    }

    return true;
  }

  static Future<bool> ensureReady(BuildContext context) async {
    final statuses = await [
      Permission.bluetoothScan,
      Permission.bluetoothConnect,
      Permission.locationWhenInUse,
    ].request();

    final denied = statuses.values.any((s) => s.isDenied || s.isPermanentlyDenied);
    if (denied) {
      if (!context.mounted) return false;
      await _showAlert(
        context,
        title: 'Location Permission Required',
        message: 'Bluetooth and Location permission was denied. '
            'Please grant them to scan for your Smart Jacket.',
      );
      return false;
    }

    final serviceStatus = await Permission.location.serviceStatus;
    if (serviceStatus != ServiceStatus.enabled) {
      if (!context.mounted) return false;
      await _showAlert(
        context,
        title: 'Location Services Disabled',
        message: 'Location information is unavailable. '
            'Please check your GPS and network connection.',
      );
      return false;
    }

    return true;
  }

  static Future<void> _showAlert(
    BuildContext context, {
    required String title,
    required String message,
  }) {
    return showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(title),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: const Text('Ask Me Later'),
          ),
          TextButton(
            onPressed: () {
              Navigator.of(ctx).pop();
              openAppSettings();
            },
            child: const Text('Open Settings'),
          ),
        ],
      ),
    );
  }
}
