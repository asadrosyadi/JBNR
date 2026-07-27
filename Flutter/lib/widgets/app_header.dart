import 'package:flutter/material.dart';

/// Top of the gradient header: app icon, title/subtitle, and a
/// Bluetooth action button that triggers a scan when tapped. Meant to
/// sit directly above [StatusBar] inside the same gradient container.
class AppHeader extends StatelessWidget {
  final VoidCallback onBluetoothTap;

  const AppHeader({super.key, required this.onBluetoothTap});

  @override
  Widget build(BuildContext context) {
    return Padding(
      padding: const EdgeInsets.fromLTRB(16, 16, 16, 16),
      child: Row(
        children: [
          ClipRRect(
            borderRadius: BorderRadius.circular(14),
            child: Container(
              width: 48,
              height: 48,
              color: Colors.white,
              child: Image.asset('assets/icon/icon.png', fit: BoxFit.cover),
            ),
          ),
          const SizedBox(width: 12),
          const Expanded(
            child: Column(
              crossAxisAlignment: CrossAxisAlignment.start,
              children: [
                Text(
                  'E-Textile Jacket',
                  style: TextStyle(
                    color: Colors.white,
                    fontSize: 18,
                    fontWeight: FontWeight.bold,
                  ),
                ),
                Text(
                  'Monitoring System',
                  style: TextStyle(color: Colors.white70, fontSize: 12),
                ),
              ],
            ),
          ),
          InkWell(
            onTap: onBluetoothTap,
            borderRadius: BorderRadius.circular(24),
            child: Container(
              padding: const EdgeInsets.all(10),
              decoration: BoxDecoration(
                color: Colors.white.withValues(alpha: 0.18),
                shape: BoxShape.circle,
              ),
              child: const Icon(Icons.bluetooth, color: Colors.white),
            ),
          ),
        ],
      ),
    );
  }
}
