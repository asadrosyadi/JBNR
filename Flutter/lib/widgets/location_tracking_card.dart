import 'package:flutter/material.dart';
import 'package:provider/provider.dart';

import '../l10n/strings.dart';
import '../models/lora_node.dart';
import '../services/format_utils.dart';
import '../services/locale_service.dart';
import '../theme/app_colors.dart';
import 'live_map_card.dart';
import 'section_card.dart';

class LocationTrackingCard extends StatelessWidget {
  final bool hasMyFix;
  final double? myLatitude;
  final double? myLongitude;
  final List<LoraNode> nodes;
  final bool isConnected;
  final bool isLoading;
  final VoidCallback onRefresh;

  const LocationTrackingCard({
    super.key,
    required this.hasMyFix,
    required this.myLatitude,
    required this.myLongitude,
    required this.nodes,
    required this.isConnected,
    required this.isLoading,
    required this.onRefresh,
  });

  int get _nodesWithFix => nodes.where((n) => n.hasFix).length;

  String _statusLabel(Strings s) {
    if (!isConnected) return s.idle;
    if (nodes.isEmpty) return s.waitingEllipsis;
    if (hasMyFix && _nodesWithFix > 0) return s.tracking;
    return s.waitingEllipsis;
  }

  Color get _statusColor {
    if (!isConnected) return AppColors.muted;
    if (hasMyFix && _nodesWithFix > 0) return AppColors.statusActive;
    return AppColors.warning;
  }

  @override
  Widget build(BuildContext context) {
    final s = Strings(context.watch<LocaleService>().language);
    return SectionCard(
      child: Column(
        crossAxisAlignment: CrossAxisAlignment.start,
        children: [
          Row(
            children: [
              const Icon(Icons.location_on, color: AppColors.primary, size: 20),
              const SizedBox(width: 8),
              Text(
                s.locationTracking,
                style: const TextStyle(fontWeight: FontWeight.bold, fontSize: 15),
              ),
              const Spacer(),
              Text(
                s.jacketsFixOf(_nodesWithFix, nodes.length),
                style: const TextStyle(fontSize: 11, color: AppColors.muted),
              ),
              const SizedBox(width: 8),
              if (isLoading)
                const SizedBox(
                  width: 16,
                  height: 16,
                  child: CircularProgressIndicator(strokeWidth: 2),
                )
              else
                InkWell(
                  onTap: onRefresh,
                  child: const Icon(Icons.refresh, color: AppColors.accentBlue),
                ),
            ],
          ),
          const SizedBox(height: 12),
          LiveMapCard(myLatitude: myLatitude, myLongitude: myLongitude, nodes: nodes),
          const SizedBox(height: 12),
          Row(
            mainAxisAlignment: MainAxisAlignment.spaceAround,
            children: [
              _InfoColumn(label: s.jacketsTracked, value: '${nodes.length}'),
              _InfoColumn(
                label: s.status,
                value: _statusLabel(s),
                valueColor: _statusColor,
              ),
            ],
          ),
          const Divider(height: 20),
          _MyLocationRow(
            myLatitude: myLatitude,
            myLongitude: myLongitude,
            hasMyFix: hasMyFix,
          ),
          if (nodes.isNotEmpty) ...[
            const Divider(height: 16),
            for (final node in nodes)
              _NodeTrackingRow(node: node, myLatitude: myLatitude, myLongitude: myLongitude),
          ],
        ],
      ),
    );
  }
}

class _MyLocationRow extends StatelessWidget {
  final double? myLatitude;
  final double? myLongitude;
  final bool hasMyFix;

  const _MyLocationRow({
    required this.myLatitude,
    required this.myLongitude,
    required this.hasMyFix,
  });

  @override
  Widget build(BuildContext context) {
    final s = Strings(context.watch<LocaleService>().language);
    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          Icon(
            Icons.my_location,
            size: 14,
            color: hasMyFix ? AppColors.statusActive : AppColors.mutedLight,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              s.phoneGpsYou,
              style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 13),
            ),
          ),
          Text(
            hasMyFix
                ? '${myLatitude!.toStringAsFixed(6)}, ${myLongitude!.toStringAsFixed(6)}'
                : s.waitingGpsFix,
            style: const TextStyle(fontSize: 12, color: AppColors.primaryDark),
          ),
        ],
      ),
    );
  }
}

class _NodeTrackingRow extends StatelessWidget {
  final LoraNode node;
  final double? myLatitude;
  final double? myLongitude;

  const _NodeTrackingRow({
    required this.node,
    required this.myLatitude,
    required this.myLongitude,
  });

  @override
  Widget build(BuildContext context) {
    final s = Strings(context.watch<LocaleService>().language);
    final distance = node.distanceFrom(myLatitude, myLongitude);
    final direction = node.compassFrom(myLatitude, myLongitude);

    return Padding(
      padding: const EdgeInsets.symmetric(vertical: 6),
      child: Row(
        children: [
          Icon(
            Icons.circle,
            size: 10,
            color: node.hasFix ? AppColors.accentBlue : AppColors.mutedLight,
          ),
          const SizedBox(width: 8),
          Expanded(
            child: Text(
              node.name,
              style: const TextStyle(fontWeight: FontWeight.w600, fontSize: 13),
            ),
          ),
          Column(
            crossAxisAlignment: CrossAxisAlignment.end,
            children: [
              Text(
                distance != null ? formatDistanceMeters(distance) : '--',
                style: const TextStyle(fontSize: 12, color: AppColors.primaryDark),
              ),
              Text(
                s.compass(direction),
                style: const TextStyle(fontSize: 11, color: AppColors.muted),
              ),
            ],
          ),
        ],
      ),
    );
  }
}

class _InfoColumn extends StatelessWidget {
  final String label;
  final String value;
  final Color? valueColor;

  const _InfoColumn({required this.label, required this.value, this.valueColor});

  @override
  Widget build(BuildContext context) {
    return Column(
      children: [
        Text(label, style: const TextStyle(fontSize: 12, color: AppColors.muted)),
        const SizedBox(height: 2),
        Text(
          value,
          style: TextStyle(
            fontSize: 14,
            fontWeight: FontWeight.bold,
            color: valueColor ?? AppColors.primaryDark,
          ),
        ),
      ],
    );
  }
}
