import 'package:flutter/material.dart';
import 'package:permission_handler/permission_handler.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import 'locale_service.dart';

class PermissionService {
  static Future<bool> ensureLocationReady(BuildContext context) async {
    final status = await Permission.locationWhenInUse.request();
    if (status.isDenied || status.isPermanentlyDenied) {
      if (!context.mounted) return false;
      final s = Strings(context.read<LocaleService>().language);
      await _showAlert(
        context,
        title: s.locationPermissionRequiredTitle,
        message: s.locationPermissionDeniedBody,
      );
      return false;
    }

    final serviceStatus = await Permission.location.serviceStatus;
    if (serviceStatus != ServiceStatus.enabled) {
      if (!context.mounted) return false;
      final s = Strings(context.read<LocaleService>().language);
      await _showAlert(
        context,
        title: s.locationServicesDisabledTitle,
        message: s.locationServicesDisabledBody,
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
      final s = Strings(context.read<LocaleService>().language);
      await _showAlert(
        context,
        title: s.locationPermissionRequiredTitle,
        message: s.bluetoothLocationPermissionDeniedBody,
      );
      return false;
    }

    final serviceStatus = await Permission.location.serviceStatus;
    if (serviceStatus != ServiceStatus.enabled) {
      if (!context.mounted) return false;
      final s = Strings(context.read<LocaleService>().language);
      await _showAlert(
        context,
        title: s.locationServicesDisabledTitle,
        message: s.locationServicesDisabledBody,
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
    final s = Strings(context.read<LocaleService>().language);
    return showDialog<void>(
      context: context,
      builder: (ctx) => AlertDialog(
        title: Text(title),
        content: Text(message),
        actions: [
          TextButton(
            onPressed: () => Navigator.of(ctx).pop(),
            child: Text(s.askMeLater),
          ),
          TextButton(
            onPressed: () {
              Navigator.of(ctx).pop();
              openAppSettings();
            },
            child: Text(s.openSettings),
          ),
        ],
      ),
    );
  }
}
